#include "pch.h"

#include "Engine.hpp"
#include "../assets/AssetBank.hpp"
#include "../compiler/Compiler.hpp"
#include "../runtime/HostCommands.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace decl_audio
{
    namespace
    {
        [[nodiscard]] compiler::Diagnostic MakeAudioBackendError(const std::filesystem::path &source_path, std::string message)
        {
            compiler::Diagnostic diagnostic;
            diagnostic.severity = compiler::DiagnosticSeverity::Error;
            diagnostic.location.file_path = source_path.string();
            diagnostic.location.object_path = "audio.backend";
            diagnostic.message = std::move(message);
            return diagnostic;
        }

        [[nodiscard]] const char *ToString(const DeclAudioBackend backend) noexcept
        {
            switch (backend)
            {
            case DECL_AUDIO_BACKEND_SILENT:
                return "silent";

            case DECL_AUDIO_BACKEND_PLATFORM_DEFAULT:
                return "platform_default";
            }

            std::terminate();
        }

        [[nodiscard]] const char *ToString(const compiler::ContainerType container_type) noexcept
        {
            switch (container_type)
            {
            case compiler::ContainerType::OneShot:
                return "oneshot";

            case compiler::ContainerType::Loop:
                return "loop";

            case compiler::ContainerType::Random:
                return "random";
            }

            std::terminate();
        }

        [[nodiscard]] std::string FormatVec3(const Vec3 &value)
        {
            std::ostringstream stream;
            stream << '(' << value.x << ", " << value.y << ", " << value.z << ')';
            return stream.str();
        }

        [[nodiscard]] const char *ToString(const bool value) noexcept
        {
            return value ? "true" : "false";
        }

        template <typename TId>
        [[nodiscard]] std::unordered_map<TId, std::string> BuildReverseLookup(const std::unordered_map<std::string, TId> &forward_lookup)
        {
            std::unordered_map<TId, std::string> reverse_lookup;
            reverse_lookup.reserve(forward_lookup.size());

            for (const auto &[name, id] : forward_lookup)
            {
                reverse_lookup.emplace(id, name);
            }

            return reverse_lookup;
        }

        template <typename TId>
        [[nodiscard]] std::string FormatNamedId(const std::unordered_map<TId, std::string> &names_by_id,
                                                const TId id,
                                                const char *label)
        {
            const auto it = names_by_id.find(id);
            if (it != names_by_id.end())
            {
                return it->second + " (" + std::string(label) + "=" + std::to_string(id) + ")";
            }

            return std::string(label) + '=' + std::to_string(id);
        }
    } // namespace

    Engine::Engine(const EngineConfig &config) noexcept
        : api_version_(DECL_AUDIO_API_VERSION),
          user_data_(nullptr),
          config(config)
    {
    }

    Engine::~Engine()
    {
        StopConfiguredAudioBackend();
    }

    bool Engine::LoadBehaviors(const char *source_path) noexcept
    {
        load_diagnostics_.clear();

        if (source_path == nullptr || source_path[0] == '\0')
        {
            return false;
        }

        compiler::CompileResult compile_result = compiler::LoadCompiledBankFromJsonFile(source_path);
        load_diagnostics_ = compile_result.diagnostics;

        if (compile_result.HasErrors())
        {
            return false;
        }

        assets::LoadResult asset_result = assets::LoadAssetBank(compile_result.bank, source_path);
        load_diagnostics_.insert(load_diagnostics_.end(), asset_result.diagnostics.begin(), asset_result.diagnostics.end());

        if (asset_result.HasErrors())
        {
            return false;
        }

        std::unique_ptr<compiler::CompiledBank> compiled_bank = std::make_unique<compiler::CompiledBank>(std::move(compile_result.bank));
        std::unique_ptr<assets::AssetBank> asset_bank = std::make_unique<assets::AssetBank>(std::move(asset_result.bank));

        StopConfiguredAudioBackend();
        compiled_bank_ = std::move(compiled_bank);
        asset_bank_ = std::move(asset_bank);
        behavior_resolver_.Reset();
        audio_runtime_.SetBanks(compiled_bank_.get(), asset_bank_.get());

        return StartConfiguredAudioBackend(source_path);
    }

    void Engine::Update() noexcept
    {
        control_runtime_.Tick();
        Vec3 listener_position;
        if (control_runtime_.ConsumeListenerPositionChange(listener_position))
        {
            audio_runtime_.Submit(playback::SetListenerPositionCommand{
                listener_position});
        }
        behavior_resolver_.Resolve(
            control_runtime_.GetWorldState(),
            *compiled_bank_,
            [this](const playback::AudioCommand &command)
            {
                audio_runtime_.Submit(command);
            });

        // TODO: clear transient tags
        control_runtime_.ClearTransientTags();
    }

    void Engine::SetTag(const char *entity_id, const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::SetTagCommand{
            std::string(entity_id),
            compiled_bank_->GetTagId(tag)});
    }

    void Engine::RemoveTag(const char *entity_id, const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::RemoveTagCommand{
            std::string(entity_id),
            compiled_bank_->GetTagId(tag)});
    }
    void Engine::SetTransientTag(const char *entity_id, const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::SetTransientTagCommand{
            std::string(entity_id),
            compiled_bank_->GetTagId(tag)}); // transient
    }
    void Engine::SetValue(const char *entity_id, const char *parameter, float value) noexcept
    {
        if (std::string_view(parameter) == "volume")
        {
            control_runtime_.Submit(runtime::SetEntityVolumeCommand{
                std::string(entity_id),
                value});
            return;
        }

        control_runtime_.Submit(runtime::SetFloatValueCommand{
            std::string(entity_id),
            compiled_bank_->GetParameterId(parameter),
            value});
    }

    void Engine::SetPosition(const char *entity_id, const float x, const float y, const float z) noexcept
    {
        control_runtime_.Submit(runtime::SetEntityPositionCommand{
            std::string(entity_id),
            Vec3{x, y, z}});
    }

    void Engine::SetListenerPosition(const float x, const float y, const float z) noexcept
    {
        control_runtime_.Submit(runtime::SetListenerPositionCommand{
            Vec3{x, y, z}});
    }

    void Engine::DestroyEntity(const char *entity_id) noexcept
    {
        control_runtime_.Submit(runtime::DestroyEntityCommand{
            std::string(entity_id)});
    }

    void Engine::GetDebugSnapshot() noexcept
    {
        const runtime::WorldState &world_state = control_runtime_.GetWorldState();
        const playback::DebugSnapshot runtime_snapshot = audio_runtime_.GetDebugSnapshot();
        const std::unordered_map<compiler::TagId, std::string> tags_by_id = compiled_bank_ != nullptr
                                                                                ? BuildReverseLookup(compiled_bank_->tag_name_to_id)
                                                                                : std::unordered_map<compiler::TagId, std::string>{};
        const std::unordered_map<compiler::ParameterId, std::string> parameters_by_id = compiled_bank_ != nullptr
                                                                                             ? BuildReverseLookup(compiled_bank_->parameter_name_to_id)
                                                                                             : std::unordered_map<compiler::ParameterId, std::string>{};
        const std::unordered_map<compiler::ProgramId, std::string> programs_by_id = compiled_bank_ != nullptr
                                                                                         ? BuildReverseLookup(compiled_bank_->program_name_to_id)
                                                                                         : std::unordered_map<compiler::ProgramId, std::string>{};

        std::vector<std::string> entity_ids;
        entity_ids.reserve(world_state.entities.size());
        for (const auto &[entity_id, entity_state] : world_state.entities)
        {
            (void)entity_state;
            entity_ids.push_back(entity_id);
        }
        std::sort(entity_ids.begin(), entity_ids.end());

        std::cout << "\n=== Engine Debug Snapshot ===\n";
        std::cout << "engine\n";
        std::cout << "  api_version: " << api_version_ << '\n';
        std::cout << "  backend: " << ToString(config.backend) << '\n';
        std::cout << "  sample_rate: " << config.sample_rate << '\n';
        std::cout << "  output_channel_count: " << config.output_channel_count << '\n';
        std::cout << "  callback_frame_count: " << config.callback_frame_count << '\n';
        std::cout << "  backend_started: " << ToString(audio_backend_ != nullptr) << '\n';
        std::cout << "  behaviors_loaded: " << ToString(compiled_bank_ != nullptr && asset_bank_ != nullptr) << '\n';
        std::cout << "  load_diagnostic_count: " << load_diagnostics_.size() << '\n';

        std::cout << "world\n";
        std::cout << "  entity_count: " << entity_ids.size() << '\n';
        for (const std::string &entity_id : entity_ids)
        {
            const runtime::EntityState &entity = world_state.GetEntity(entity_id);
            std::vector<std::string> persistent_tags;
            persistent_tags.reserve(entity.tags.size());
            for (const compiler::TagId tag_id : entity.tags)
            {
                persistent_tags.push_back(FormatNamedId(tags_by_id, tag_id, "tag_id"));
            }
            std::sort(persistent_tags.begin(), persistent_tags.end());

            std::vector<std::string> transient_tags;
            transient_tags.reserve(entity.transient_tags.size());
            for (const compiler::TagId tag_id : entity.transient_tags)
            {
                transient_tags.push_back(FormatNamedId(tags_by_id, tag_id, "tag_id"));
            }
            std::sort(transient_tags.begin(), transient_tags.end());

            std::vector<std::pair<std::string, float>> float_values;
            float_values.reserve(entity.float_values.size());
            for (const auto &[parameter_id, value] : entity.float_values)
            {
                float_values.emplace_back(FormatNamedId(parameters_by_id, parameter_id, "parameter_id"), value);
            }
            std::sort(float_values.begin(), float_values.end(), [](const auto &lhs, const auto &rhs)
                      { return lhs.first < rhs.first; });

            std::cout << "  entity: " << entity_id << '\n';
            std::cout << "    persistent_tags: " << persistent_tags.size() << '\n';
            for (const std::string &tag_name : persistent_tags)
            {
                std::cout << "      " << tag_name << '\n';
            }
            std::cout << "    transient_tags: " << transient_tags.size() << '\n';
            for (const std::string &tag_name : transient_tags)
            {
                std::cout << "      " << tag_name << '\n';
            }
            std::cout << "    float_values: " << float_values.size() << '\n';
            for (const auto &[parameter_name, value] : float_values)
            {
                std::cout << "      " << parameter_name << " = " << value << '\n';
            }
            std::cout << "    volume: ";
            if (entity.HasVolume())
            {
                std::cout << entity.GetVolume() << '\n';
            }
            else
            {
                std::cout << "<unset>\n";
            }
            std::cout << "    position: ";
            if (entity.HasPosition())
            {
                std::cout << FormatVec3(entity.GetPosition()) << '\n';
            }
            else
            {
                std::cout << "<unset>\n";
            }
        }

        std::cout << "audio_runtime\n";
        std::cout << "  listener_position: " << FormatVec3(runtime_snapshot.listener_position) << '\n';
        std::cout << "  root_seed: 0x" << std::hex << runtime_snapshot.root_seed << std::dec << '\n';
        std::cout << "  max_instances: " << runtime_snapshot.max_instances << '\n';
        std::cout << "  max_block_frames: " << runtime_snapshot.max_block_frames << '\n';
        std::cout << "  active_instance_count: " << runtime_snapshot.active_instance_count << '\n';
        std::cout << "  pending_audio_commands: <not introspected; commands are applied during render>\n";

        std::vector<playback::InstanceDebugSnapshot> instances = runtime_snapshot.instances;
        std::sort(instances.begin(), instances.end(), [](const auto &lhs, const auto &rhs)
                  { return lhs.instance_id < rhs.instance_id; });

        for (const playback::InstanceDebugSnapshot &instance : instances)
        {
            std::cout << "  instance: " << instance.instance_id << '\n';
            std::cout << "    program: " << FormatNamedId(programs_by_id, instance.program_id, "program_id") << '\n';
            std::cout << "    cursor: " << instance.cursor << '\n';
            std::cout << "    container_type: " << ToString(instance.container_type) << '\n';
            std::cout << "    volume: " << instance.volume << '\n';
            std::cout << "    position: " << FormatVec3(instance.position) << '\n';
            std::cout << "    stop_requested: " << ToString(instance.stop_requested) << '\n';
            std::cout << "    sample_position: " << instance.sample_position << '\n';
            if (instance.container_type == compiler::ContainerType::Loop)
            {
                std::cout << "    remaining_loops: " << instance.remaining_loops << '\n';
            }
            if (instance.container_type == compiler::ContainerType::Random)
            {
                std::cout << "    picked_asset_slot: " << instance.picked_asset_slot << '\n';
            }

            if (compiled_bank_ != nullptr)
            {
                const compiler::CompiledProgram &program = compiled_bank_->GetProgram(instance.program_id);
                const compiler::CompiledContainer &container = compiled_bank_->containers[program.first_container + instance.cursor];
                const std::span<const compiler::AssetId> asset_ids = compiled_bank_->GetContainerAssets(container);
                std::cout << "    assets: " << asset_ids.size() << '\n';
                for (std::size_t asset_index = 0; asset_index < asset_ids.size(); ++asset_index)
                {
                    const compiler::AssetId asset_id = asset_ids[asset_index];
                    std::cout << "      [" << asset_index << "] " << compiled_bank_->GetAssetPath(asset_id)
                              << " (asset_id=" << asset_id << ")\n";
                }
            }
        }

        std::cout << "=== End Debug Snapshot ===\n";
    }

    bool Engine::StartConfiguredAudioBackend(const char *source_path) noexcept
    {
        if (config.backend == DECL_AUDIO_BACKEND_SILENT)
        {
            audio_backend_.reset();
            return true;
        }

        audio_backend_ = backends::CreateAudioDeviceBackend(config.backend);
        std::string error_message;
        if (!audio_backend_->Start(audio_runtime_, config, error_message))
        {
            load_diagnostics_.push_back(MakeAudioBackendError(source_path, std::move(error_message)));
            audio_backend_.reset();
            return false;
        }

        return true;
    }

    void Engine::StopConfiguredAudioBackend() noexcept
    {
        if (audio_backend_ == nullptr)
        {
            return;
        }

        audio_backend_->Stop();
        audio_backend_.reset();
    }

} // namespace decl_audio
