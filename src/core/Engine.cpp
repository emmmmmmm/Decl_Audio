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
                         static_cast<std::size_t>(config.command_queue_capacity),
                         config.max_program_node_count,
                         config.max_program_concurrent_voices,
                         config.max_program_parameter_slot_count),
          api_version_(DECL_AUDIO_API_VERSION),
          user_data_(nullptr),
          config(config)
    {
        // The backend runs continuously while banks come and go - an empty mix is
        // just silence. Adding/removing banks never stops it (gapless).
        if (config.backend != DECL_AUDIO_BACKEND_SILENT)
        {
            (void)StartConfiguredAudioBackend("<engine init>");
        }
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

        if (HasActiveBank(source_path))
            return true; // already loaded - idempotent, skip the compile entirely

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

        return AddBank(std::move(compile_result.bank), std::move(asset_result.bank), source_path);
    }

    bool Engine::LoadBank(const char *bank_path) noexcept
    {
        load_diagnostics_.clear();

        if (bank_path == nullptr || bank_path[0] == '\0')
            return false;

        if (HasActiveBank(bank_path))
            return true; // already loaded - idempotent, skip reading the file

        serialization::LoadBankResult result = serialization::LoadBankFromFile(bank_path);
        return ConsumeBankResult(std::move(result), bank_path);
    }

    bool Engine::LoadBankAsync(const char *bank_path) noexcept
    {
        if (bank_path == nullptr || bank_path[0] == '\0')
            return false;

        if (HasActiveBank(bank_path))
            return true; // already loaded - idempotent, no worker spawned

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

        return AddBank(std::move(result.compiled_bank), std::move(result.asset_bank), source_path);
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

    bool Engine::AddBank(compiler::CompiledBank &&compiled, assets::AssetBank &&assets, const char *source_path) noexcept
    {
        // Backstop for the idempotency the entry points already enforce: a sync load
        // can race an in-flight async load of the same path and reach here after it
        // became Active. Drop the duplicate; the path is already loaded (success).
        if (HasActiveBank(source_path))
            return true;

        // Validate the bank fits the per-instance storage caps (audio-owned storage
        // is fixed at construction and never grows).
        if (compiled.max_program_node_count > config.max_program_node_count ||
            compiled.max_program_concurrent_voices > config.max_program_concurrent_voices ||
            compiled.max_program_parameter_slot_count > config.max_program_parameter_slot_count)
        {
            const Diagnostic &diag = load_diagnostics_.emplace_back(
                MakeError(source_path, "bank.capacity", "bank exceeds configured program storage caps"));
            PushLog("[error] " + FormatSourceLocation(diag.location) + ": " + diag.message);
            return false;
        }

        // Find a free slot. A retiring/draining bank still occupies its slot, so this
        // can legitimately fail under slot pressure - reject with a diagnostic.
        std::size_t slot = kMaxBanks;
        for (std::size_t candidate = 0; candidate < kMaxBanks; ++candidate)
        {
            if (banks_[candidate] == nullptr)
            {
                slot = candidate;
                break;
            }
        }
        if (slot == kMaxBanks)
        {
            const Diagnostic &diag = load_diagnostics_.emplace_back(
                MakeError(source_path, "bank.slots", "no free bank slot (too many banks loaded)"));
            PushLog("[error] " + FormatSourceLocation(diag.location) + ": " + diag.message);
            return false;
        }

        auto loaded = std::make_unique<LoadedBank>();
        loaded->id = BankId{static_cast<std::uint32_t>(slot), slot_generation_[slot]++};
        loaded->compiled = std::move(compiled);
        loaded->assets = std::move(assets);
        loaded->source_path = (source_path != nullptr) ? source_path : "";
        loaded->status = BankStatus::Active;

        // Intern + remap this bank's vocabulary to global ids (content ids stay local).
        vocabulary_.MergeBank(loaded->compiled);

        // Publish into the audio slot table BEFORE the resolver can emit any
        // CreateInstance for it; the command ring then carries the happens-before.
        audio_runtime_.InstallBank(loaded->id, &loaded->compiled, &loaded->assets);
        banks_[slot] = std::move(loaded);

        PushLog("Bank loaded.");
        return true;
    }

    void Engine::UnloadBank(const char *bank_path) noexcept
    {
        if (bank_path == nullptr || bank_path[0] == '\0')
            return;

        control_runtime_.Submit(runtime::UnloadBankCommand{std::string(bank_path)});
    }

    const LoadedBank *Engine::FirstLoadedBank() const noexcept
    {
        for (const std::unique_ptr<LoadedBank> &bank : banks_)
        {
            if (bank != nullptr)
                return bank.get();
        }
        return nullptr;
    }

    bool Engine::HasActiveBank(const char *source_path) const noexcept
    {
        if (source_path == nullptr)
            return false;

        for (const std::unique_ptr<LoadedBank> &bank : banks_)
        {
            if (bank != nullptr && bank->status == BankStatus::Active && bank->source_path == source_path)
                return true;
        }
        return false;
    }

    // to be on its own thread in the future i guess...!
    void Engine::Update() noexcept
    {
        PollAsyncLoad(); // wire any finished async load before resolving this tick

        control_runtime_.Tick(); // drain control queue

        // Mark unloaded banks Retiring before resolving so the resolver skips them.
        ProcessPendingUnloads();

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

        // World state keeps accumulating from the drained commands regardless of
        // which banks are loaded; the resolver gathers candidates across every
        // active bank. An empty bank set is fine - it just resolves to nothing.
        ResolveLoadedBanks();

        // transient tags
        control_runtime_.ClearTransientTags();

        // Reclaim buckets the audio thread has finished draining.
        SweepDrainedBanks();
    }

    void Engine::ResolveLoadedBanks() noexcept
    {
        runtime::ResolverBankView views[kMaxBanks];
        std::size_t view_count = 0;
        for (const std::unique_ptr<LoadedBank> &bank : banks_)
        {
            if (bank == nullptr)
                continue;
            views[view_count++] = runtime::ResolverBankView{
                bank->id, &bank->compiled, bank->status == BankStatus::Retiring};
        }

        behavior_resolver_.Resolve(
            control_runtime_.GetWorldState(),
            std::span<const runtime::ResolverBankView>(views, view_count),
            [this](const playback::AudioCommand &command)
            {
                audio_runtime_.Submit(command);
            });
    }

    void Engine::ProcessPendingUnloads() noexcept
    {
        for (const std::string &path : control_runtime_.TakePendingUnloads())
        {
            for (const std::unique_ptr<LoadedBank> &bank : banks_)
            {
                if (bank == nullptr || bank->status != BankStatus::Active || bank->source_path != path)
                    continue;

                // Mark Retiring: the resolver stops gathering candidates from it and
                // we drop its bindings here (the RetireBankCommand stops the audio
                // instances in one shot). Then route the retire down the command ring.
                bank->status = BankStatus::Retiring;
                behavior_resolver_.DropBank(bank->id);
                audio_runtime_.Submit(playback::RetireBankCommand{bank->id});
                break; // unload one matching bank per request
            }
        }
    }

    void Engine::SweepDrainedBanks() noexcept
    {
        for (std::unique_ptr<LoadedBank> &bank : banks_)
        {
            if (bank == nullptr || bank->status != BankStatus::Retiring)
                continue;
            if (!audio_runtime_.IsSlotDrained(bank->id))
                continue;

            // Audio confirmed Drained: no live instances, no pending creates. Clear
            // the audio slot pointers, then free the bucket. The slot becomes reusable.
            audio_runtime_.FreeBankSlot(bank->id);
            bank.reset();
        }
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
