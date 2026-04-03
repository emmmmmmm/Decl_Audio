#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "../RingBuffer.hpp"
#include "../assets/AssetBank.hpp"
#include "../compiler/CompiledBank.hpp"
#include "AudioCommands.hpp"

namespace decl_audio::playback
{
    struct OneShotState final
    {
        std::uint64_t sample_position = 0;
    };

    struct LoopState final
    {
        std::uint64_t sample_position = 0;
        std::int32_t remaining_loops = 0;
    };

    struct RandomState final
    {
        std::uint32_t picked_asset_slot = 0;
        std::uint64_t sample_position = 0;
    };

    using ActiveContainerState = std::variant<OneShotState, LoopState, RandomState>;

    struct ProgramInstance final
    {
        InstanceId instance_id = 0;
        const compiler::CompiledProgram *compiled = nullptr;
        std::uint32_t cursor = 0;
        float volume = 1.0f;
        Vec3 position{};
        bool stop_requested = false;
        ActiveContainerState current{};
    };

    struct InstanceSnapshot final
    {
        InstanceId instance_id = 0;
        compiler::ProgramId program_id = 0;
        std::uint32_t cursor = 0;
        compiler::ContainerType container_type = compiler::ContainerType::OneShot;
        float volume = 1.0f;
        Vec3 position{};
        bool stop_requested = false;
    };

    struct InstanceDebugSnapshot final
    {
        InstanceId instance_id = 0;
        compiler::ProgramId program_id = 0;
        std::uint32_t cursor = 0;
        compiler::ContainerType container_type = compiler::ContainerType::OneShot;
        float volume = 1.0f;
        Vec3 position{};
        bool stop_requested = false;
        std::uint64_t sample_position = 0;
        std::int32_t remaining_loops = 0;
        std::uint32_t picked_asset_slot = 0;
    };

    struct DebugSnapshot final
    {
        Vec3 listener_position{};
        std::uint64_t root_seed = 0;
        std::size_t max_instances = 0;
        std::uint32_t max_block_frames = 0;
        std::size_t active_instance_count = 0;
        std::vector<InstanceDebugSnapshot> instances;
    };

    struct ListenerState final
    {
        Vec3 position{};
    };

    class AudioRuntime final
    {
    public:
        static constexpr std::size_t CommandQueueCapacity = 1024;
        static constexpr std::size_t DefaultMaxInstances = 256;
        static constexpr std::uint32_t DefaultMaxBlockFrames = 262144;
        static constexpr std::uint32_t OutputChannelCount = 2;

        explicit AudioRuntime(std::uint64_t root_seed = 0xC0FFEEULL,
                              std::size_t max_instances = DefaultMaxInstances,
                              std::uint32_t max_block_frames = DefaultMaxBlockFrames);

        void SetBanks(const compiler::CompiledBank *compiled_bank, const assets::AssetBank *asset_bank) noexcept;
        void Clear() noexcept;
        void Submit(const AudioCommand &command);
        void Render(float *output, std::uint32_t frames) noexcept;

        [[nodiscard]] std::size_t ActiveInstanceCount() const noexcept
        {
            return instances_.size();
        }

        [[nodiscard]] bool TryGetInstanceSnapshot(InstanceId instance_id, InstanceSnapshot &snapshot) const noexcept;
        [[nodiscard]] DebugSnapshot GetDebugSnapshot() const noexcept;
        [[nodiscard]] const Vec3 &GetListenerPositionForTesting() const noexcept
        {
            return listener_.position;
        }

    private:
        void ApplyPendingCommands() noexcept;
        void Apply(const CreateInstanceCommand &command) noexcept;
        void Apply(const SetVolumeCommand &command) noexcept;
        void Apply(const SetPositionCommand &command) noexcept;
        void Apply(const RequestStopCommand &command) noexcept;
        void Apply(const SetListenerPositionCommand &command) noexcept;

        [[nodiscard]] std::uint32_t RenderProgramInstance(ProgramInstance &instance, float *output, std::uint32_t frames) noexcept;
        [[nodiscard]] std::uint32_t RenderCurrentContainer(ProgramInstance &instance, float *output, std::uint32_t frames) noexcept;
        [[nodiscard]] ActiveContainerState MakeContainerState(const ProgramInstance &instance, std::uint32_t cursor) const noexcept;
        [[nodiscard]] const compiler::CompiledContainer &GetCompiledContainer(const ProgramInstance &instance) const noexcept;
        [[nodiscard]] const compiler::CompiledContainer &GetCompiledContainer(const ProgramInstance &instance, std::uint32_t cursor) const noexcept;
        [[nodiscard]] std::size_t FindInstanceIndex(InstanceId instance_id) const noexcept;
        [[nodiscard]] std::uint64_t DeriveContainerSeed(InstanceId instance_id, compiler::ProgramId program_id, std::uint32_t cursor) const noexcept;

        RingBuffer<AudioCommand, CommandQueueCapacity> commands_;
        std::vector<ProgramInstance> instances_;
        std::vector<float> scratch_;
        const compiler::CompiledBank *compiled_bank_ = nullptr;
        const assets::AssetBank *asset_bank_ = nullptr;
        ListenerState listener_{};
        std::uint64_t root_seed_ = 0;
        std::size_t max_instances_ = 0;
        std::uint32_t max_block_frames_ = 0;
    };
} // namespace decl_audio::playback
