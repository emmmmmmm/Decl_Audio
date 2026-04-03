#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "Decl_Audio/Decl_Audio.h"
#include "../assets/AssetBank.hpp"
#include "../backends/AudioDeviceBackend.hpp"
#include "../backends/StubBackend.hpp"
#include "../compiler/CompiledBank.hpp"
#include "ConfigSupport.hpp"
#include "../playback/AudioCommands.hpp"
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
        virtual ~Engine();

        Engine(const Engine &) = delete;
        Engine &operator=(const Engine &) = delete;
        Engine(Engine &&) = delete;
        Engine &operator=(Engine &&) = delete;

        virtual bool LoadBehaviors(const char *source_path) noexcept;
        virtual void Update() noexcept;
        virtual void SetTag(const char *entity_id, const char *tag) noexcept;
        virtual void RemoveTag(const char *entity_id, const char *tag) noexcept;
        virtual void SetValue(const char *entity_id, const char *parameter, float value) noexcept;
        virtual void SetPosition(const char *entity_id, float x, float y, float z) noexcept;
        virtual void SetListenerPosition(float x, float y, float z) noexcept;
        virtual void DestroyEntity(const char *entity_id) noexcept;

        // Phase 4 test seam. The C API stays unchanged until resolver/backend work is in place.
        virtual void SubmitCreateInstanceForTesting(playback::InstanceId instance_id,
                                                    compiler::ProgramId program_id,
                                                    float volume = 1.0f,
                                                    Vec3 position = {}) noexcept;
        virtual void SubmitSetVolumeForTesting(playback::InstanceId instance_id, float volume) noexcept;
        virtual void SubmitSetPositionForTesting(playback::InstanceId instance_id, Vec3 position) noexcept;
        virtual void SubmitSetListenerPositionForTesting(Vec3 position) noexcept;
        virtual void SubmitRequestStopForTesting(playback::InstanceId instance_id) noexcept;
        virtual void RenderAudioForTesting(float *output, std::uint32_t frames) noexcept;
        virtual void PumpAudioForTesting(std::uint32_t frames) noexcept;

        [[nodiscard]] std::size_t GetActiveAudioInstanceCountForTesting() const noexcept
        {
            return audio_runtime_.ActiveInstanceCount();
        }

        [[nodiscard]] bool TryGetAudioInstanceSnapshotForTesting(playback::InstanceId instance_id,
                                                                 playback::InstanceSnapshot &snapshot) const noexcept
        {
            return audio_runtime_.TryGetInstanceSnapshot(instance_id, snapshot);
        }

        [[nodiscard]] const Vec3 &GetListenerPositionForTesting() const noexcept
        {
            return audio_runtime_.GetListenerPositionForTesting();
        }

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

        [[nodiscard]] const assets::AssetBank &GetAssetBank() const noexcept
        {
            return *asset_bank_;
        };

        [[nodiscard]] std::span<const compiler::Diagnostic> GetLoadDiagnostics() const noexcept
        {
            return load_diagnostics_;
        };

    private:
        [[nodiscard]] bool StartConfiguredAudioBackend(const char *source_path) noexcept;
        void StopConfiguredAudioBackend() noexcept;

        std::unique_ptr<compiler::CompiledBank> compiled_bank_;
        std::unique_ptr<assets::AssetBank> asset_bank_;
        std::unique_ptr<backends::AudioDeviceBackend> audio_backend_;
        std::vector<compiler::Diagnostic> load_diagnostics_;
        runtime::ControlRuntime control_runtime_;
        runtime::BehaviorResolver behavior_resolver_;
        playback::AudioRuntime audio_runtime_;
        backends::StubBackend stub_backend_;
        AudioConfig audio_config_{};
        uint32_t api_version_;
        void *user_data_;
    };
} // namespace decl_audio
