#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "Decl_Audio/Decl_Audio.h"
#include "../assets/AssetBank.hpp"
#include "../backends/AudioDeviceBackend.hpp"
#include "../compiler/CompiledBank.hpp"
#include "../playback/AudioRuntime.hpp"
#include "../runtime/BehaviorResolver.hpp"
#include "../runtime/ControlRuntime.hpp"
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
        void Update() noexcept;
        void RenderAudioForTesting(float *output, std::uint32_t frames) noexcept;
        void SetTag(const char *entity_id, const char *tag) noexcept;
        void SetTransientTag(const char *entity_id, const char *tag) noexcept;
        void RemoveTag(const char *entity_id, const char *tag) noexcept;
        void SetValue(const char *entity_id, const char *parameter, float value) noexcept;
        void SetPosition(const char *entity_id, float x, float y, float z) noexcept;
        void SetListenerPosition(float x, float y, float z) noexcept;
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
        [[nodiscard]] const assets::AssetBank &GetAssetBank() const noexcept
        {
            return *asset_bank_;
        };
        [[nodiscard]] const compiler::CompiledBank &GetCompiledBank() const noexcept
        {
            return *compiled_bank_;
        };
        [[nodiscard]] const compiler::CompiledBank *TryGetCompiledBank() const noexcept
        {
            return compiled_bank_.get();
        };
        [[nodiscard]] const assets::AssetBank *TryGetAssetBank() const noexcept
        {
            return asset_bank_.get();
        };
        [[nodiscard]] bool HasStartedBackend() const noexcept
        {
            return audio_backend_ != nullptr;
        };

        [[nodiscard]] std::span<const compiler::Diagnostic> GetLoadDiagnostics() const noexcept
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

        std::unique_ptr<compiler::CompiledBank> compiled_bank_;
        std::unique_ptr<assets::AssetBank> asset_bank_;
        std::unique_ptr<backends::AudioDeviceBackend> audio_backend_;
        std::vector<compiler::Diagnostic> load_diagnostics_;
        runtime::ControlRuntime control_runtime_;
        runtime::BehaviorResolver behavior_resolver_;
        playback::AudioRuntime audio_runtime_;
        uint32_t api_version_;
        void *user_data_;
        EngineConfig config;
    };
} // namespace decl_audio
