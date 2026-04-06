#pragma once

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Engine.hpp"
#include "../compiler/Compiler.hpp"

namespace decl_audio::DebugUtils
{
    namespace detail
    {
        [[nodiscard]] inline const char *ToString(const DeclAudioBackend backend) noexcept
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

        [[nodiscard]] inline const char *ToString(const compiler::NodeType node_type) noexcept
        {
            switch (node_type)
            {
            case compiler::NodeType::Sequence:
                return "sequence";

            case compiler::NodeType::Select:
                return "select";

            case compiler::NodeType::Blend:
                return "blend";

            case compiler::NodeType::OneShot:
                return "oneshot";

            case compiler::NodeType::Loop:
                return "loop";

            case compiler::NodeType::Random:
                return "random";
            }

            std::terminate();
        }

        [[nodiscard]] inline const char *ToString(const bool value) noexcept
        {
            return value ? "true" : "false";
        }

        [[nodiscard]] inline std::string FormatVec3(const Vec3 &value)
        {
            std::ostringstream stream;
            stream << '(' << value.x << ", " << value.y << ", " << value.z << ')';
            return stream.str();
        }

        template <typename TId>
        [[nodiscard]] inline std::unordered_map<TId, std::string> BuildReverseLookup(const std::unordered_map<std::string, TId> &forward_lookup)
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
        [[nodiscard]] inline std::string FormatNamedId(const std::unordered_map<TId, std::string> &names_by_id,
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
    } // namespace detail

    inline void PrintSnapshot(const Engine *engine) noexcept
    {
        const runtime::WorldState &world_state = engine->GetWorldState();
        const playback::DebugSnapshot runtime_snapshot = engine->GetDebugSnapshot();
        const compiler::CompiledBank *compiled_bank = engine->TryGetCompiledBank();
        const assets::AssetBank *asset_bank = engine->TryGetAssetBank();
        const EngineConfig config = engine->GetConfig();

        const std::unordered_map<compiler::TagId, std::string> tags_by_id =
            compiled_bank != nullptr ? detail::BuildReverseLookup(compiled_bank->tag_name_to_id) : std::unordered_map<compiler::TagId, std::string>{};
        const std::unordered_map<compiler::ParameterId, std::string> parameters_by_id =
            compiled_bank != nullptr ? detail::BuildReverseLookup(compiled_bank->parameter_name_to_id) : std::unordered_map<compiler::ParameterId, std::string>{};
        const std::unordered_map<compiler::ProgramId, std::string> programs_by_id =
            compiled_bank != nullptr ? detail::BuildReverseLookup(compiled_bank->program_name_to_id) : std::unordered_map<compiler::ProgramId, std::string>{};

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
        std::cout << "  api_version: " << engine->GetApiVersion() << '\n';
        std::cout << "  backend: " << detail::ToString(config.backend) << '\n';
        std::cout << "  sample_rate: " << config.sample_rate << '\n';
        std::cout << "  output_channel_count: " << config.output_channel_count << '\n';
        std::cout << "  callback_frame_count: " << config.callback_frame_count << '\n';
        std::cout << "  max_instances: " << config.max_instances << '\n';
        std::cout << "  max_block_frames: " << config.max_block_frames << '\n';
        std::cout << "  backend_started: " << detail::ToString(engine->HasStartedBackend()) << '\n';
        std::cout << "  behaviors_loaded: " << detail::ToString(compiled_bank != nullptr && asset_bank != nullptr) << '\n';
        std::cout << "  load_diagnostic_count: " << engine->GetLoadDiagnostics().size() << '\n';

        std::cout << "world\n";
        std::cout << "  entity_count: " << entity_ids.size() << '\n';
        for (const std::string &entity_id : entity_ids)
        {
            const runtime::EntityState &entity = world_state.GetEntity(entity_id);
            std::vector<std::string> persistent_tags;
            persistent_tags.reserve(entity.tags.size());
            for (const compiler::TagId tag_id : entity.tags)
            {
                persistent_tags.push_back(detail::FormatNamedId(tags_by_id, tag_id, "tag_id"));
            }
            std::sort(persistent_tags.begin(), persistent_tags.end());

            std::vector<std::string> transient_tags;
            transient_tags.reserve(entity.transient_tags.size());
            for (const compiler::TagId tag_id : entity.transient_tags)
            {
                transient_tags.push_back(detail::FormatNamedId(tags_by_id, tag_id, "tag_id"));
            }
            std::sort(transient_tags.begin(), transient_tags.end());

            std::vector<std::pair<std::string, float>> float_values;
            float_values.reserve(entity.float_values.size());
            for (const auto &[parameter_id, value] : entity.float_values)
            {
                float_values.emplace_back(detail::FormatNamedId(parameters_by_id, parameter_id, "parameter_id"), value);
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
                std::cout << detail::FormatVec3(entity.GetPosition()) << '\n';
            }
            else
            {
                std::cout << "<unset>\n";
            }
        }

        std::cout << "audio_runtime\n";
        std::cout << "  listener_position: " << detail::FormatVec3(runtime_snapshot.listener_position) << '\n';
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
            std::cout << "    program: " << detail::FormatNamedId(programs_by_id, instance.program_id, "program_id") << '\n';
            std::cout << "    volume: " << instance.volume << '\n';
            std::cout << "    position: " << detail::FormatVec3(instance.position) << '\n';
            std::cout << "    stop_requested: " << detail::ToString(instance.stop_requested) << '\n';
            std::cout << "    active_voice_count: " << instance.active_voice_count << '\n';
            std::cout << "    nodes: " << instance.nodes.size() << '\n';
            for (const playback::NodeDebugSnapshot &node : instance.nodes)
            {
                std::cout << "      node " << node.node_id
                          << " type=" << detail::ToString(node.type)
                          << " entered=" << detail::ToString(node.entered)
                          << " finished=" << detail::ToString(node.finished)
                          << " chosen_child=" << node.chosen_child
                          << " active_voice_count=" << node.active_voice_count
                          << '\n';
            }
            std::cout << "    voices: " << instance.voices.size() << '\n';
            for (const playback::VoiceDebugSnapshot &voice : instance.voices)
            {
                std::cout << "      leaf_node=" << voice.leaf_node_id
                          << " type=" << detail::ToString(voice.leaf_type)
                          << " sample_position=" << voice.sample_position
                          << " remaining_loops=" << voice.remaining_loops
                          << " picked_asset_slot=" << voice.picked_asset_slot
                          << '\n';
            }
        }

        if (compiled_bank != nullptr)
        {
            std::cout << compiler::DumpCompiledBank(*compiled_bank);
        }

        std::cout << "=== End Debug Snapshot ===\n";
    }
} // namespace decl_audio::DebugUtils
