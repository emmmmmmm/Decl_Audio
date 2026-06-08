#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "Decl_Audio/Decl_Audio.h"
#include "../assets/AssetBank.hpp"
#include "../backends/AudioDeviceBackend.hpp"
#include "../compiler/CompiledBank.hpp"
#include "BankId.hpp"
#include "BankSerializer.hpp"
#include "LoadedBank.hpp"
#include "../core/RingBuffer.hpp"
#include "../playback/AudioRuntime.hpp"
#include "../runtime/BehaviorResolver.hpp"
#include "../runtime/ControlRuntime.hpp"
#include "../runtime/VocabularyRegistry.hpp"
#include "../runtime/WorldState.hpp"

namespace decl_audio
{
    class Engine
    {
    public:
        explicit Engine(const EngineConfig &config) noexcept;
        ~Engine();

        Engine(const Engine &) = delete;
        Engine &operator=(const Engine &) = delete;
        Engine(Engine &&) = delete;
        Engine &operator=(Engine &&) = delete;

        bool LoadBehaviors(const char *source_path) noexcept;
        bool LoadBank(const char *bank_path) noexcept;
        // Kicks off a background deserialize and returns immediately. Returns false
        // if a load is already in flight (no concurrent loads). The finished bank is
        // wired in on the control thread during a later Update(); fire-and-forget.
        bool LoadBankAsync(const char *bank_path) noexcept;
        // Unload the bank loaded from `bank_path`. Routed through the control ring;
        // the bank's instances fade out and the bucket is freed once drained.
        void UnloadBank(const char *bank_path) noexcept;
        void Update() noexcept;
        void RenderAudioForTesting(float *output, std::uint32_t frames) noexcept;
        [[nodiscard]] bool TryDequeueLog(std::string &message) noexcept;
        void SetTag(const char *entity_id, const char *tag) noexcept;
        void SetTransientTag(const char *entity_id, const char *tag) noexcept;
        void RemoveTag(const char *entity_id, const char *tag) noexcept;
        void SetValue(const char *entity_id, const char *parameter, float value) noexcept;
        void SetGlobalTag(const char *tag) noexcept;
        void RemoveGlobalTag(const char *tag) noexcept;
        void SetGlobalValue(const char *param, float value) noexcept;
        void SetPosition(const char *entity_id, float x, float y, float z) noexcept;
        void SetListenerPosition(float x, float y, float z) noexcept;
        void SetMasterGain(float gain) noexcept;
        void DestroyEntity(const char *entity_id) noexcept;

        [[nodiscard]] uint32_t GetApiVersion() const noexcept
        {
            return api_version_;
        };

        [[nodiscard]] void *GetUserData() const noexcept
        {
            return user_data_;
        };

        [[nodiscard]] const runtime::WorldState &GetWorldState() const noexcept
        {
            return control_runtime_.GetWorldState();
        };
        [[nodiscard]] const playback::DebugSnapshot GetDebugSnapshot() const noexcept
        {
            return audio_runtime_.GetDebugSnapshot();
        };
        // Per-bank accessor. Returns nullptr if no live bank occupies that slot/id.
        [[nodiscard]] const LoadedBank *TryGetBank(BankId bank_id) const noexcept
        {
            const LoadedBank *bank = banks_[bank_id.slot].get();
            return (bank != nullptr && bank->id == bank_id) ? bank : nullptr;
        };
        // Convenience accessors for the first loaded bank - handy for single-bank
        // tests/debug. Multi-bank callers should use TryGetBank(BankId).
        [[nodiscard]] const compiler::CompiledBank *TryGetCompiledBank() const noexcept
        {
            const LoadedBank *bank = FirstLoadedBank();
            return bank != nullptr ? &bank->compiled : nullptr;
        };
        [[nodiscard]] const assets::AssetBank *TryGetAssetBank() const noexcept
        {
            const LoadedBank *bank = FirstLoadedBank();
            return bank != nullptr ? &bank->assets : nullptr;
        };
        [[nodiscard]] const assets::AssetBank &GetAssetBank() const noexcept
        {
            return *TryGetAssetBank();
        };
        [[nodiscard]] const compiler::CompiledBank &GetCompiledBank() const noexcept
        {
            return *TryGetCompiledBank();
        };
        [[nodiscard]] bool HasStartedBackend() const noexcept
        {
            return audio_backend_ != nullptr;
        };

        [[nodiscard]] std::span<const decl_audio::Diagnostic> GetLoadDiagnostics() const noexcept
        {
            return load_diagnostics_;
        };
        [[nodiscard]] EngineConfig GetConfig() const noexcept
        {
            return config;
        };

    private:
        [[nodiscard]] bool
        StartConfiguredAudioBackend(const char *source_path) noexcept;
        void StopConfiguredAudioBackend() noexcept;
        // Intern + remap the bank's vocabulary, install it into a free slot, and make
        // it resolvable. No backend stop - gapless. Returns false (with a diagnostic)
        // if the bank exceeds the storage caps or no slot is free.
        bool AddBank(compiler::CompiledBank &&compiled, assets::AssetBank &&assets, const char *source_path) noexcept;
        // Shared tail of the binary-bank load (sync LoadBank + async completion):
        // record diagnostics, then add the bank unless the result errored.
        bool ConsumeBankResult(serialization::LoadBankResult &&result, const char *source_path) noexcept;
        // Control-thread side of async loading: if the worker finished, join it and
        // wire the result. No-op when nothing is pending. Called from Update().
        void PollAsyncLoad() noexcept;
        // Run the resolver over every loaded bank (skipping retiring ones).
        void ResolveLoadedBanks() noexcept;
        // Drain host unload requests: mark the named bank Retiring, drop its
        // bindings, and submit the RetireBankCommand to the audio thread.
        void ProcessPendingUnloads() noexcept;
        // Free buckets whose slots the audio thread has confirmed Drained.
        void SweepDrainedBanks() noexcept;
        [[nodiscard]] const LoadedBank *FirstLoadedBank() const noexcept;
        // True if a bank loaded from this path is currently Active. Path is a bank's
        // identity: re-loading an already-loaded bank is an idempotent no-op success.
        // (A Retiring bank with the same path does not count - it is on its way out,
        // so a fresh load brings the content back.)
        [[nodiscard]] bool HasActiveBank(const char *source_path) const noexcept;
        void PushLog(std::string message);
        void PushDiagnostics(std::span<const decl_audio::Diagnostic> diagnostics);

        std::unique_ptr<backends::AudioDeviceBackend> audio_backend_;
        // Control-thread-only bank registry, indexed by BankId.slot. A null slot is
        // free; a retiring bank occupies its slot until the audio thread drains it.
        // unique_ptr keeps each bank address-stable for the audio thread's raw
        // pointers. slot_generation_ bumps on each (re)use so stale ids mismatch.
        std::unique_ptr<LoadedBank> banks_[kMaxBanks];
        std::uint32_t slot_generation_[kMaxBanks] = {};

        // Async load handoff. The worker writes pending_load_result_ then sets
        // load_result_ready_ (release); the control thread reads it (acquire) in
        // PollAsyncLoad. load_in_flight_ stays set from LoadBankAsync until the
        // result is consumed, gating concurrent loads.
        std::thread load_worker_;
        std::atomic<bool> load_in_flight_{false};
        std::atomic<bool> load_result_ready_{false};
        serialization::LoadBankResult pending_load_result_;
        std::string pending_load_path_;
        std::vector<decl_audio::Diagnostic> load_diagnostics_;
        RingBuffer<std::string> host_log_queue_;
        runtime::VocabularyRegistry vocabulary_;
        runtime::ControlRuntime control_runtime_;
        runtime::BehaviorResolver behavior_resolver_;
        playback::AudioRuntime audio_runtime_;
        uint32_t api_version_;
        void *user_data_;
        EngineConfig config;
    };
} // namespace decl_audio
