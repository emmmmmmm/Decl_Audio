#include "pch.h"

#include "Engine.hpp"
#include "../assets/AssetBank.hpp"
#include "../compiler/Compiler.hpp"
#include "../runtime/HostCommands.hpp"
#include "../core/DebugUtils.hpp"
#include <filesystem>
#include <string_view>

namespace decl_audio
{

    Engine::Engine(const EngineConfig &config) noexcept
        : control_runtime_(static_cast<std::size_t>(config.host_queue_capacity)),
          audio_runtime_(0xC0FFEEULL,
                         static_cast<std::size_t>(config.max_instances),
                         config.max_block_frames,
                         config.output_channel_count,
                         static_cast<std::size_t>(config.command_queue_capacity)),
          api_version_(DECL_AUDIO_API_VERSION),
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
            // TODO: Dump diagnostics?
            return false;
        }

        assets::LoadResult asset_result = assets::LoadAssetBank(compile_result.bank, source_path);
        load_diagnostics_.insert(load_diagnostics_.end(), asset_result.diagnostics.begin(), asset_result.diagnostics.end());

        if (asset_result.HasErrors())
        {
            // TODO: Dump diagnostics?
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

    // to be on its own thread in the future i guess...!
    void Engine::Update() noexcept
    {
        control_runtime_.Tick(); // drain control queue
        Vec3 listener_position;
        if (control_runtime_.ListenerPositionChanged(listener_position))
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

        // transient tags
        control_runtime_.ClearTransientTags();
    }

    void Engine::RenderAudioForTesting(float *output, const std::uint32_t frames) noexcept
    {
        audio_runtime_.Render(output, frames);
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
            load_diagnostics_.push_back(MakeError(source_path, "audio.backend", std::move(error_message)));
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
