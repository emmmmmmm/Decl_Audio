#include "pch.h"

#include "Engine.hpp"
#include "BankSerializer.hpp"
#include "../assets/AssetBank.hpp"
#include "../compiler/Compiler.hpp"
#include "../runtime/HostCommands.hpp"
#include "../core/DebugUtils.hpp"
#include <filesystem>
#include <string_view>

namespace decl_audio
{

    Engine::Engine(const EngineConfig &config) noexcept
        : host_log_queue_(static_cast<std::size_t>(config.host_queue_capacity)),
          control_runtime_(vocabulary_, static_cast<std::size_t>(config.host_queue_capacity)),
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
        // Join the load worker before tearing anything down it might still touch.
        if (load_worker_.joinable())
        {
            load_worker_.join();
        }
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
        PushDiagnostics(compile_result.diagnostics);

        if (compile_result.HasErrors())
        {
            return false;
        }

        assets::LoadResult asset_result = assets::LoadAssetBank(compile_result.bank, source_path);
        load_diagnostics_.insert(load_diagnostics_.end(), asset_result.diagnostics.begin(), asset_result.diagnostics.end());
        PushDiagnostics(asset_result.diagnostics);

        if (asset_result.HasErrors())
        {
            return false;
        }

        auto compiled_bank = std::make_unique<compiler::CompiledBank>(std::move(compile_result.bank));
        auto asset_bank = std::make_unique<assets::AssetBank>(std::move(asset_result.bank));
        return WireLoadedBanks(std::move(compiled_bank), std::move(asset_bank), source_path);
    }

    bool Engine::LoadBank(const char *bank_path) noexcept
    {
        load_diagnostics_.clear();

        if (bank_path == nullptr || bank_path[0] == '\0')
            return false;

        serialization::LoadBankResult result = serialization::LoadBankFromFile(bank_path);
        return ConsumeBankResult(std::move(result), bank_path);
    }

    bool Engine::LoadBankAsync(const char *bank_path) noexcept
    {
        if (bank_path == nullptr || bank_path[0] == '\0')
            return false;

        bool expected = false;
        if (!load_in_flight_.compare_exchange_strong(expected, true))
            return false; // a load is already in flight

        // The previous worker is already joined (load_in_flight_ only clears in
        // PollAsyncLoad, after the join), but guard before reassigning the handle.
        if (load_worker_.joinable())
        {
            load_worker_.join();
        }

        load_result_ready_.store(false, std::memory_order_relaxed);
        pending_load_path_ = bank_path;

        load_worker_ = std::thread(
            [this, path = std::string(bank_path)]() noexcept
            {
                // Worker: only the slow, pure deserialize. Touches no engine state
                // beyond the result + ready flag. Diagnostics ride in the result and
                // are drained by the control thread - never the SPSC host log queue.
                pending_load_result_ = serialization::LoadBankFromFile(path.c_str());
                load_result_ready_.store(true, std::memory_order_release);
            });

        return true;
    }

    bool Engine::ConsumeBankResult(serialization::LoadBankResult &&result, const char *source_path) noexcept
    {
        load_diagnostics_ = result.diagnostics;
        PushDiagnostics(result.diagnostics);

        if (result.HasErrors())
            return false;

        auto compiled_bank = std::make_unique<compiler::CompiledBank>(std::move(result.compiled_bank));
        auto asset_bank = std::make_unique<assets::AssetBank>(std::move(result.asset_bank));
        return WireLoadedBanks(std::move(compiled_bank), std::move(asset_bank), source_path);
    }

    void Engine::PollAsyncLoad() noexcept
    {
        if (!load_result_ready_.load(std::memory_order_acquire))
            return;

        load_worker_.join();
        load_result_ready_.store(false, std::memory_order_relaxed);

        // Fast, stateful wiring on the control thread - the same work synchronous
        // LoadBank does, just deferred off the worker.
        (void)ConsumeBankResult(std::move(pending_load_result_), pending_load_path_.c_str());

        load_in_flight_.store(false, std::memory_order_release);
    }

    bool Engine::WireLoadedBanks(
        std::unique_ptr<compiler::CompiledBank> compiled_bank,
        std::unique_ptr<assets::AssetBank> asset_bank,
        const char *source_path) noexcept
    {
        StopConfiguredAudioBackend();
        compiled_bank_ = std::move(compiled_bank);
        asset_bank_ = std::move(asset_bank);
        behavior_resolver_.Reset();
        vocabulary_.AdoptBank(*compiled_bank_);
        audio_runtime_.SetBanks(compiled_bank_.get(), asset_bank_.get());

        PushLog("Behavior loading: done.");
        return StartConfiguredAudioBackend(source_path);
    }

    // to be on its own thread in the future i guess...!
    void Engine::Update() noexcept
    {
        PollAsyncLoad(); // wire any finished async load before resolving this tick

        control_runtime_.Tick(); // drain control queue
        Vec3 listener_position;
        if (control_runtime_.ListenerPositionChanged(listener_position))
        {
            audio_runtime_.Submit(playback::SetListenerPositionCommand{
                listener_position});
        }

        float master_gain;
        if (control_runtime_.MasterGainChanged(master_gain))
        {
            audio_runtime_.Submit(playback::SetMasterGainCommand{master_gain});
        }

        // No bank yet (e.g. an async load still in flight): world state keeps
        // accumulating from the drained commands; the resolver starts matching once
        // a bank is wired. Declarative facts self-heal - load order stops mattering.
        if (compiled_bank_ != nullptr)
        {
            behavior_resolver_.Resolve(
                control_runtime_.GetWorldState(),
                *compiled_bank_,
                BankId{0u, 0u}, // single-bank: the one loaded bank lives in slot 0
                [this](const playback::AudioCommand &command)
                {
                    audio_runtime_.Submit(command);
                });
        }

        // transient tags
        control_runtime_.ClearTransientTags();
    }

    void Engine::RenderAudioForTesting(float *output, const std::uint32_t frames) noexcept
    {
        audio_runtime_.Render(output, frames);
    }

    bool Engine::TryDequeueLog(std::string &message) noexcept
    {
        return host_log_queue_.pop(message);
    }

    void Engine::SetTag(const char *entity_id, const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::SetTagCommand{
            std::string(entity_id),
            std::string(tag)});
    }

    void Engine::RemoveTag(const char *entity_id, const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::RemoveTagCommand{
            std::string(entity_id),
            std::string(tag)});
    }

    void Engine::SetTransientTag(const char *entity_id, const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::SetTransientTagCommand{
            std::string(entity_id),
            std::string(tag)}); // transient
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
            std::string(parameter),
            value});
    }

    void Engine::SetGlobalTag(const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::SetGlobalTagCommand{
            std::string(tag)});
    }
    void Engine::RemoveGlobalTag(const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::RemoveGlobalTagCommand{
            std::string(tag)});
    }
    void Engine::SetGlobalValue(const char *param, float value) noexcept
    {
        control_runtime_.Submit(runtime::SetGlobalFloatValueCommand{
            std::string(param),
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

    void Engine::SetMasterGain(const float gain) noexcept
    {
        control_runtime_.Submit(runtime::SetMasterGainCommand{gain});
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
            auto &diag = load_diagnostics_.emplace_back(MakeError(source_path, "audio.backend", std::move(error_message)));
            PushLog("[error] " + FormatSourceLocation(diag.location) + ": " + diag.message);
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

    void Engine::PushLog(std::string message)
    {
        if (!host_log_queue_.push(std::move(message)))
            std::terminate();
    }

    void Engine::PushDiagnostics(std::span<const decl_audio::Diagnostic> diagnostics)
    {
        for (const decl_audio::Diagnostic &diagnostic : diagnostics)
        {
            const std::string prefix = diagnostic.severity == DiagnosticSeverity::Error ? "[error] " : "[warning] ";
            PushLog(prefix + FormatSourceLocation(diagnostic.location) + ": " + diagnostic.message);
        }
    }

} // namespace decl_audio
