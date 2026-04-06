#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "../core/RingBuffer.hpp"
#include "../assets/AssetBank.hpp"
#include "../compiler/CompiledBank.hpp"
#include "AudioCommands.hpp"

namespace decl_audio::playback
{
    struct NodeRuntimeState final
    {
        bool entered = false;
        bool finished = false;
        std::int32_t chosen_child = -1;
        std::uint16_t active_voice_count = 0;
    };

    struct VoiceState final
    {
        compiler::NodeId leaf_node = std::numeric_limits<compiler::NodeId>::max();
        std::uint64_t sample_position = 0;
        std::int32_t remaining_loops = 0;
        std::uint32_t picked_asset_slot = 0;
        bool active = false;
    };

    struct ProgramInstance final
    {
        InstanceId instance_id = 0;
        const compiler::CompiledProgram *compiled = nullptr;
        float volume = 1.0f;
        Vec3 position{};
        bool stop_requested = false;
        std::uint32_t active_voice_count = 0;
        std::span<float> parameter_slots;
        std::span<NodeRuntimeState> node_state;
        std::span<VoiceState> voices;
    };

    struct InstanceSnapshot final
    {
        InstanceId instance_id = 0;
        compiler::ProgramId program_id = 0;
        float volume = 1.0f;
        Vec3 position{};
        bool stop_requested = false;
        std::uint32_t active_voice_count = 0;
    };

    struct NodeDebugSnapshot final
    {
        compiler::NodeId node_id = 0;
        compiler::NodeType type = compiler::NodeType::Sequence;
        bool entered = false;
        bool finished = false;
        std::int32_t chosen_child = -1;
        std::uint16_t active_voice_count = 0;
    };

    struct VoiceDebugSnapshot final
    {
        compiler::NodeId leaf_node_id = 0;
        compiler::NodeType leaf_type = compiler::NodeType::OneShot;
        std::uint64_t sample_position = 0;
        std::int32_t remaining_loops = 0;
        std::uint32_t picked_asset_slot = 0;
    };

    struct InstanceDebugSnapshot final
    {
        InstanceId instance_id = 0;
        compiler::ProgramId program_id = 0;
        float volume = 1.0f;
        Vec3 position{};
        bool stop_requested = false;
        std::uint32_t active_voice_count = 0;
        std::vector<NodeDebugSnapshot> nodes;
        std::vector<VoiceDebugSnapshot> voices;
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
        explicit AudioRuntime(std::uint64_t root_seed = 0xC0FFEEULL,
                              std::size_t max_instances = 256,
                              std::uint32_t max_block_frames = 4096,
                              std::uint32_t out_channel_count = 2,
                              std::size_t command_queue_capacity = 1024);

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
        static constexpr compiler::NodeId kInvalidNodeId = std::numeric_limits<compiler::NodeId>::max();

        void ApplyPendingCommands() noexcept;
        void Apply(const CreateInstanceCommand &command) noexcept;
        void Apply(const SetVolumeCommand &command) noexcept;
        void Apply(const SetPositionCommand &command) noexcept;
        void Apply(const SetParameterCommand &command) noexcept;
        void Apply(const RequestStopCommand &command) noexcept;
        void Apply(const SetListenerPositionCommand &command) noexcept;

        [[nodiscard]] bool RenderProgramInstance(ProgramInstance &instance, float *output, std::uint32_t frames) noexcept;
        [[nodiscard]] std::uint32_t ComputeSegmentFrames(const ProgramInstance &instance, std::uint32_t frames_remaining) const noexcept;
        void RenderVoice(ProgramInstance &instance, VoiceState &voice, float *output, std::uint32_t frames) noexcept;
        void ActivateVoice(ProgramInstance &instance, compiler::NodeId leaf_node) noexcept;
        void RetireVoice(ProgramInstance &instance, std::uint32_t voice_index) noexcept;
        void EnterNode(ProgramInstance &instance, compiler::NodeId node_id) noexcept;
        void TryFinishNode(ProgramInstance &instance, compiler::NodeId node_id) noexcept;
        [[nodiscard]] std::uint64_t ComputeVoiceTerminalFrames(const ProgramInstance &instance, const VoiceState &voice) const noexcept;
        [[nodiscard]] float ComputeVoiceGain(const ProgramInstance &instance, compiler::NodeId leaf_node) const noexcept;
        [[nodiscard]] const compiler::CompiledNode &GetCompiledNode(compiler::NodeId node_id) const noexcept;
        [[nodiscard]] std::span<const compiler::NodeId> GetNodeChildren(compiler::NodeId node_id) const noexcept;
        [[nodiscard]] std::span<const compiler::AssetId> GetNodeAssets(compiler::NodeId node_id) const noexcept;
        [[nodiscard]] std::uint16_t FindProgramParameterSlot(const ProgramInstance &instance, compiler::ParameterId parameter_id) const noexcept;
        [[nodiscard]] std::size_t FindInstanceIndex(InstanceId instance_id) const noexcept;
        [[nodiscard]] std::uint64_t DeriveNodeSeed(InstanceId instance_id, compiler::ProgramId program_id, compiler::NodeId node_id) const noexcept;
        void ResizeStorageForBank() noexcept;

        RingBuffer<AudioCommand> commands_;
        std::vector<ProgramInstance> instances_;
        std::vector<float> scratch_;
        std::vector<NodeRuntimeState> node_state_storage_;
        std::vector<VoiceState> voice_storage_;
        std::vector<float> parameter_storage_;
        const compiler::CompiledBank *compiled_bank_ = nullptr;
        const assets::AssetBank *asset_bank_ = nullptr;
        ListenerState listener_{};
        std::uint64_t root_seed_ = 0;
        std::size_t max_instances_ = 0;
        std::uint32_t max_block_frames_ = 0;
        std::uint32_t out_channel_count_ = 2;
    };
} // namespace decl_audio::playback
