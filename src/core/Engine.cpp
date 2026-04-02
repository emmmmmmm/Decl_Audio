#include "pch.h"

#include "Engine.hpp"
#include "../assets/AssetBank.hpp"
#include "../compiler/Compiler.hpp"
#include "../runtime/HostCommands.hpp"

#include <string>

namespace decl_audio
{
    Engine::Engine(const EngineConfig &config) noexcept
        : api_version_(config.api_version),
          user_data_(config.user_data)
    {
        // spin up audio thread and set up commandbuffers
    }

    Engine::~Engine() = default;

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

        compiled_bank_ = std::make_unique<compiler::CompiledBank>(std::move(compile_result.bank));
        asset_bank_ = std::make_unique<assets::AssetBank>(std::move(asset_result.bank));
        behavior_resolver_.Reset();
        audio_runtime_.SetBanks(compiled_bank_.get(), asset_bank_.get());

        return true;
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
        control_runtime_.Submit(runtime::SetFloatValueCommand{
            std::string(entity_id),
            compiled_bank_->GetParameterId(parameter),
            value});
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

} // namespace decl_audio
