#include "pch.h"

#include "Engine.hpp"
#include "../assets/AssetBank.hpp"
#include "../compiler/Compiler.hpp"
#include "../runtime/HostCommands.hpp"

#include <filesystem>
#include <string>
#include <string_view>

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
    } // namespace

    Engine::Engine(const EngineConfig &config) noexcept
        : api_version_(config.api_version),
          audio_config_(ResolveAudioConfig(config)),
          user_data_(config.user_data)
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
        behavior_resolver_.Resolve(
            control_runtime_.GetWorldState(),
            *compiled_bank_,
            [this](const playback::AudioCommand &command)
            {
                audio_runtime_.Submit(command);
            });
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

    void Engine::DestroyEntity(const char *entity_id) noexcept
    {
        control_runtime_.Submit(runtime::DestroyEntityCommand{
            std::string(entity_id)});
    }

    void Engine::SubmitCreateInstanceForTesting(const playback::InstanceId instance_id,
                                                const compiler::ProgramId program_id,
                                                const float volume,
                                                const Vec3 position) noexcept
    {
        audio_runtime_.Submit(playback::CreateInstanceCommand{
            instance_id,
            program_id,
            position,
            volume});
    }

    void Engine::SubmitSetVolumeForTesting(const playback::InstanceId instance_id, const float volume) noexcept
    {
        audio_runtime_.Submit(playback::SetVolumeCommand{
            instance_id,
            volume});
    }

    void Engine::SubmitSetPositionForTesting(const playback::InstanceId instance_id, const Vec3 position) noexcept
    {
        audio_runtime_.Submit(playback::SetPositionCommand{
            instance_id,
            position});
    }

    void Engine::SubmitRequestStopForTesting(const playback::InstanceId instance_id) noexcept
    {
        audio_runtime_.Submit(playback::RequestStopCommand{
            instance_id});
    }

    void Engine::RenderAudioForTesting(float *output, const std::uint32_t frames) noexcept
    {
        audio_runtime_.Render(output, frames);
    }

    void Engine::PumpAudioForTesting(const std::uint32_t frames) noexcept
    {
        stub_backend_.Pump(audio_runtime_, frames);
    }

    bool Engine::StartConfiguredAudioBackend(const char *source_path) noexcept
    {
        if (audio_config_.backend == DECL_AUDIO_BACKEND_SILENT)
        {
            audio_backend_.reset();
            return true;
        }

        audio_backend_ = backends::CreateAudioDeviceBackend(audio_config_.backend);
        std::string error_message;
        if (!audio_backend_->Start(audio_runtime_, audio_config_, error_message))
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
