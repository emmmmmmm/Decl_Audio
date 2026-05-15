#include "pch.h"

#include "AudioRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace decl_audio::playback
{
    namespace
    {
        constexpr std::size_t kNotFound = std::numeric_limits<std::size_t>::max();
        constexpr float kQuarterTurn = 0.78539816339744830962f;
        constexpr std::uint16_t kInvalidParameterSlot = std::numeric_limits<std::uint16_t>::max();

        [[nodiscard]] std::uint64_t MixSeed64(std::uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ULL;
            value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
            value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
            return value ^ (value >> 31U);
        }

        struct StereoMixGains final
        {
            float left = 1.0f;
            float right = 1.0f;
        };

        [[nodiscard]] StereoMixGains ComputeSpatialMixGains(const compiler::CompiledSpatializationSettings &spatialization,
                                                            const Vec3 &source_position,
                                                            const Vec3 &listener_position) noexcept
        {
            if (spatialization.mode == compiler::SpatializationMode::None)
            {
                return StereoMixGains{};
            }

            const Vec3 relative = Vec3::subtract(source_position, listener_position);
            const float distance = relative.magnitude();

            float attenuation = 1.0f;
            switch (spatialization.attenuation)
            {
            case compiler::AttenuationMode::Linear:
                if (distance <= spatialization.min_distance)
                {
                    attenuation = 1.0f;
                }
                else if (distance >= spatialization.max_distance)
                {
                    attenuation = 0.0f;
                }
                else
                {
                    attenuation = 1.0f - ((distance - spatialization.min_distance) / (spatialization.max_distance - spatialization.min_distance));
                }
                break;
            }

            float pan = 0.0f;
            if (distance > 0.0f)
            {
                pan = std::clamp(relative.x / distance, -1.0f, 1.0f);
            }

            const float angle = (pan + 1.0f) * kQuarterTurn;
            return StereoMixGains{
                std::cos(angle) * attenuation,
                std::sin(angle) * attenuation};
        }
    } // namespace

    AudioRuntime::AudioRuntime(const std::uint64_t root_seed,
                               const std::size_t max_instances,
                               const std::uint32_t max_block_frames,
                               const std::uint32_t out_channel_count,
                               const std::size_t command_queue_capacity)
        : commands_(command_queue_capacity),
          root_seed_(root_seed),
          max_instances_(max_instances),
          max_block_frames_(max_block_frames),
          out_channel_count_(out_channel_count)
    {
        instances_.reserve(max_instances_);
        scratch_.resize(static_cast<std::size_t>(max_block_frames_) * out_channel_count);
    }

    void AudioRuntime::SetBanks(const compiler::CompiledBank *compiled_bank, const assets::AssetBank *asset_bank) noexcept
    {
        Clear();
        compiled_bank_ = compiled_bank;
        asset_bank_ = asset_bank;
        ResizeStorageForBank();
    }

    void AudioRuntime::ResizeStorageForBank() noexcept
    {
        node_state_storage_.clear();
        voice_storage_.clear();
        parameter_storage_.clear();

        if (compiled_bank_ == nullptr)
        {
            return;
        }

        node_state_storage_.resize(max_instances_ * static_cast<std::size_t>(compiled_bank_->max_program_node_count));
        voice_storage_.resize(max_instances_ * static_cast<std::size_t>(compiled_bank_->max_program_concurrent_voices));
        parameter_storage_.resize(max_instances_ * static_cast<std::size_t>(compiled_bank_->max_program_parameter_slot_count));

        free_slices_.clear();
        free_slices_.reserve(max_instances_);
        for (std::size_t i = max_instances_; i > 0; --i)
        {
            free_slices_.push_back(i - 1);
        }
    }

    void AudioRuntime::Clear() noexcept
    {
        instances_.clear();

        free_slices_.clear();
        free_slices_.reserve(max_instances_);
        for (std::size_t i = max_instances_; i > 0; --i)
        {
            free_slices_.push_back(i - 1);
        }

        AudioCommand command;
        while (commands_.pop(command))
        {
        }
    }

    void AudioRuntime::Submit(const AudioCommand &command)
    {
        if (!commands_.push(command))
        {
            std::terminate();
        }
    }

    void AudioRuntime::Render(float *output, const std::uint32_t frames) noexcept
    {
        if (frames > max_block_frames_)
        {
            std::terminate();
        }

        std::fill_n(output, static_cast<std::size_t>(frames) * out_channel_count_, 0.0f);

        ApplyPendingCommands();

        std::size_t instance_index = 0;
        while (instance_index < instances_.size())
        {
            ProgramInstance &instance = instances_[instance_index];
            std::fill_n(scratch_.data(), static_cast<std::size_t>(frames) * out_channel_count_, 0.0f);

            bool keep_instance = RenderProgramInstance(instance, scratch_.data(), frames);

            if (instance.stop_requested && instance.compiled->stop_mode == compiler::StopMode::Immediate)
            {
                if (instance.compiled->stop_fade_frames == 0)
                {
                    keep_instance = false;
                }
                else if (instance.stop_fade_frames_remaining > 0)
                {
                    const float total_f = static_cast<float>(instance.compiled->stop_fade_frames);
                    const std::uint32_t fade_start = instance.stop_fade_frames_remaining;
                    for (std::uint32_t f = 0; f < frames; ++f)
                    {
                        const float gain = f < fade_start
                            ? static_cast<float>(fade_start - f) / total_f
                            : 0.0f;
                        const std::size_t idx = static_cast<std::size_t>(f) * out_channel_count_;
                        scratch_[idx + 0] *= gain;
                        scratch_[idx + 1] *= gain;
                    }
                    instance.stop_fade_frames_remaining = fade_start > frames ? fade_start - frames : 0;
                    if (instance.stop_fade_frames_remaining == 0)
                    {
                        keep_instance = false;
                    }
                }
                else
                {
                    keep_instance = false;
                }
            }

            if (instance.start_fade_frames_remaining > 0)
            {
                const float total_f = static_cast<float>(instance.compiled->start_fade_frames);
                const std::uint32_t remaining = instance.start_fade_frames_remaining;
                const std::uint32_t elapsed_at_block_start = instance.compiled->start_fade_frames - remaining;
                for (std::uint32_t f = 0; f < frames; ++f)
                {
                    const float gain = f < remaining
                        ? static_cast<float>(elapsed_at_block_start + f) / total_f
                        : 1.0f;
                    const std::size_t idx = static_cast<std::size_t>(f) * out_channel_count_;
                    scratch_[idx + 0] *= gain;
                    scratch_[idx + 1] *= gain;
                }
                instance.start_fade_frames_remaining = remaining > frames ? remaining - frames : 0;
            }

            const StereoMixGains mix_gains = ComputeSpatialMixGains(instance.compiled->spatialization, instance.position, listener_.position);
            for (std::uint32_t frame_index = 0; frame_index < frames; ++frame_index)
            {
                const std::size_t sample_index = static_cast<std::size_t>(frame_index) * out_channel_count_;
                output[sample_index + 0] += scratch_[sample_index + 0] * mix_gains.left;
                output[sample_index + 1] += scratch_[sample_index + 1] * mix_gains.right;
            }

            if (!keep_instance)
            {
                free_slices_.push_back(instances_[instance_index].slice_index);
                instances_[instance_index] = instances_.back();
                instances_.pop_back();
                continue;
            }

            ++instance_index;
        }

        if (master_gain_ != 1.0f)
        {
            const std::size_t sample_count = static_cast<std::size_t>(frames) * out_channel_count_;
            for (std::size_t i = 0; i < sample_count; ++i)
            {
                output[i] *= master_gain_;
            }
        }
    }

    bool AudioRuntime::TryGetInstanceSnapshot(const InstanceId instance_id, InstanceSnapshot &snapshot) const noexcept
    {
        const std::size_t instance_index = FindInstanceIndex(instance_id);
        if (instance_index == kNotFound)
        {
            return false;
        }

        const ProgramInstance &instance = instances_[instance_index];
        snapshot.instance_id = instance.instance_id;
        snapshot.program_id = instance.compiled->id;
        snapshot.volume = instance.volume;
        snapshot.position = instance.position;
        snapshot.stop_requested = instance.stop_requested;
        snapshot.active_voice_count = instance.active_voice_count;
        return true;
    }

    DebugSnapshot AudioRuntime::GetDebugSnapshot() const noexcept
    {
        DebugSnapshot snapshot;
        snapshot.listener_position = listener_.position;
        snapshot.root_seed = root_seed_;
        snapshot.max_instances = max_instances_;
        snapshot.max_block_frames = max_block_frames_;
        snapshot.active_instance_count = instances_.size();
        snapshot.instances.reserve(instances_.size());

        for (const ProgramInstance &instance : instances_)
        {
            InstanceDebugSnapshot instance_snapshot;
            instance_snapshot.instance_id = instance.instance_id;
            instance_snapshot.program_id = instance.compiled->id;
            instance_snapshot.volume = instance.volume;
            instance_snapshot.position = instance.position;
            instance_snapshot.stop_requested = instance.stop_requested;
            instance_snapshot.active_voice_count = instance.active_voice_count;
            instance_snapshot.nodes.reserve(instance.compiled->node_count);

            for (std::uint32_t node_offset = 0; node_offset < instance.compiled->node_count; ++node_offset)
            {
                const compiler::NodeId node_id = instance.compiled->first_node + node_offset;
                const compiler::CompiledNode &node = GetCompiledNode(node_id);
                const NodeRuntimeState &state = instance.node_state[node_offset];
                instance_snapshot.nodes.push_back(NodeDebugSnapshot{
                    node_id,
                    node.type,
                    state.entered,
                    state.finished,
                    state.chosen_child,
                    state.active_voice_count});
            }

            for (const VoiceState &voice : instance.voices)
            {
                if (!voice.active)
                {
                    continue;
                }

                instance_snapshot.voices.push_back(VoiceDebugSnapshot{
                    voice.leaf_node,
                    GetCompiledNode(voice.leaf_node).type,
                    voice.sample_position,
                    voice.remaining_loops,
                    voice.picked_asset_slot});
            }

            snapshot.instances.push_back(std::move(instance_snapshot));
        }

        return snapshot;
    }

    void AudioRuntime::ApplyPendingCommands() noexcept
    {
        AudioCommand command;
        while (commands_.pop(command))
        {
            std::visit(
                [this](const auto &typed_command)
                {
                    Apply(typed_command);
                },
                command);
        }
    }

    void AudioRuntime::Apply(const CreateInstanceCommand &command) noexcept
    {
        if (compiled_bank_ == nullptr || asset_bank_ == nullptr)
        {
            std::terminate();
        }

        if (FindInstanceIndex(command.instance_id) != kNotFound)
        {
            std::terminate();
        }

        if (instances_.size() >= max_instances_)
        {
            std::terminate();
        }

        const compiler::CompiledProgram &compiled_program = compiled_bank_->GetProgram(command.program_id);
        const std::size_t slice_index = free_slices_.back();
        free_slices_.pop_back();

        auto make_node_span = [&](const std::uint32_t count) noexcept -> std::span<NodeRuntimeState>
        {
            if (count == 0)
            {
                return {};
            }

            return std::span<NodeRuntimeState>(
                node_state_storage_.data() + (slice_index * static_cast<std::size_t>(compiled_bank_->max_program_node_count)),
                count);
        };

        auto make_voice_span = [&](const std::uint32_t count) noexcept -> std::span<VoiceState>
        {
            if (count == 0)
            {
                return {};
            }

            return std::span<VoiceState>(
                voice_storage_.data() + (slice_index * static_cast<std::size_t>(compiled_bank_->max_program_concurrent_voices)),
                count);
        };

        auto make_parameter_span = [&](const std::uint32_t count) noexcept -> std::span<float>
        {
            if (count == 0)
            {
                return {};
            }

            return std::span<float>(
                parameter_storage_.data() + (slice_index * static_cast<std::size_t>(compiled_bank_->max_program_parameter_slot_count)),
                count);
        };

        ProgramInstance instance;
        instance.instance_id = command.instance_id;
        instance.compiled = &compiled_program;
        instance.slice_index = slice_index;
        instance.volume = command.volume;
        instance.position = command.position;
        instance.stop_requested = false;
        instance.active_voice_count = 0;
        instance.stop_fade_frames_remaining = 0;
        instance.start_fade_frames_remaining = compiled_program.start_fade_frames;
        instance.node_state = make_node_span(compiled_program.node_count);
        instance.voices = make_voice_span(compiled_program.max_concurrent_voices);
        instance.parameter_slots = make_parameter_span(compiled_program.parameter_slot_count);

        std::fill(instance.node_state.begin(), instance.node_state.end(), NodeRuntimeState{});
        std::fill(instance.voices.begin(), instance.voices.end(), VoiceState{});
        std::fill(instance.parameter_slots.begin(), instance.parameter_slots.end(), 0.0f);

        instances_.push_back(instance);
        EnterNode(instances_.back(), compiled_program.root_node);
    }

    void AudioRuntime::Apply(const SetVolumeCommand &command) noexcept
    {
        const std::size_t instance_index = FindInstanceIndex(command.instance_id);
        if (instance_index == kNotFound)
        {
            return;
        }

        instances_[instance_index].volume = command.volume;
    }

    void AudioRuntime::Apply(const SetPositionCommand &command) noexcept
    {
        const std::size_t instance_index = FindInstanceIndex(command.instance_id);
        if (instance_index == kNotFound)
        {
            return;
        }

        instances_[instance_index].position = command.position;
    }

    void AudioRuntime::Apply(const SetParameterCommand &command) noexcept
    {
        const std::size_t instance_index = FindInstanceIndex(command.instance_id);
        if (instance_index == kNotFound)
        {
            return;
        }

        ProgramInstance &instance = instances_[instance_index];
        const std::uint16_t parameter_slot = FindProgramParameterSlot(instance, command.parameter_id);
        if (parameter_slot == kInvalidParameterSlot)
        {
            std::terminate();
        }

        instance.parameter_slots[parameter_slot] = command.value;
    }

    void AudioRuntime::Apply(const RequestStopCommand &command) noexcept
    {
        const std::size_t instance_index = FindInstanceIndex(command.instance_id);
        if (instance_index == kNotFound)
        {
            return;
        }

        ProgramInstance &instance = instances_[instance_index];
        instance.stop_requested = true;

        if (instance.compiled->stop_mode == compiler::StopMode::Immediate)
        {
            // Start fade-out; voices keep playing untouched until the fade kills the instance.
            instance.stop_fade_frames_remaining = instance.compiled->stop_fade_frames;
        }
        else
        {
            // Graceful: let the current loop pass finish, then advance the sequence normally.
            for (VoiceState &voice : instance.voices)
            {
                if (!voice.active)
                {
                    continue;
                }

                if (GetCompiledNode(voice.leaf_node).type == compiler::NodeType::Loop)
                {
                    voice.remaining_loops = 0;
                }
            }
        }
    }

    void AudioRuntime::Apply(const SetListenerPositionCommand &command) noexcept
    {
        listener_.position = command.position;
    }

    void AudioRuntime::Apply(const SetMasterGainCommand &command) noexcept
    {
        master_gain_ = command.gain;
    }

    bool AudioRuntime::RenderProgramInstance(ProgramInstance &instance, float *output, const std::uint32_t frames) noexcept
    {
        const std::uint32_t root_offset = instance.compiled->root_node - instance.compiled->first_node;
        std::uint32_t written = 0;

        while (written < frames)
        {
            if (instance.active_voice_count == 0)
            {
                if (instance.node_state[root_offset].finished)
                {
                    break;
                }

                std::terminate();
            }

            const std::uint32_t segment_frames = ComputeSegmentFrames(instance, frames - written);
            if (segment_frames == 0)
            {
                std::terminate();
            }

            for (VoiceState &voice : instance.voices)
            {
                if (!voice.active)
                {
                    continue;
                }

                RenderVoice(instance,
                            voice,
                            output + static_cast<std::size_t>(written) * out_channel_count_,
                            segment_frames);
            }

            written += segment_frames;

            std::size_t voice_index = 0;
            while (voice_index < instance.voices.size())
            {
                if (!instance.voices[voice_index].active)
                {
                    ++voice_index;
                    continue;
                }

                if (ComputeVoiceTerminalFrames(instance, instance.voices[voice_index]) == 0)
                {
                    RetireVoice(instance, static_cast<std::uint32_t>(voice_index));
                    continue;
                }

                ++voice_index;
            }
        }

        return !(instance.node_state[root_offset].finished && instance.active_voice_count == 0);
    }

    std::uint32_t AudioRuntime::ComputeSegmentFrames(const ProgramInstance &instance, const std::uint32_t frames_remaining) const noexcept
    {
        std::uint64_t segment_frames = frames_remaining;
        for (const VoiceState &voice : instance.voices)
        {
            if (!voice.active)
            {
                continue;
            }

            segment_frames = std::min(segment_frames, ComputeVoiceTerminalFrames(instance, voice));
        }

        return static_cast<std::uint32_t>(segment_frames);
    }

    void AudioRuntime::RenderVoice(ProgramInstance &instance, VoiceState &voice, float *output, const std::uint32_t frames) noexcept
    {
        const compiler::CompiledNode &node = GetCompiledNode(voice.leaf_node);
        const std::span<const compiler::AssetId> asset_ids = GetNodeAssets(voice.leaf_node);
        const float gain = ComputeVoiceGain(instance, voice.leaf_node);

        auto add_frames = [&](const assets::DecodedBuffer &buffer,
                              std::uint64_t &sample_position,
                              float *target_output,
                              const std::uint32_t frames_requested) noexcept -> std::uint32_t
        {
            const std::uint64_t remaining_frames = buffer.frame_count - sample_position;
            const std::uint32_t frames_to_write = static_cast<std::uint32_t>(std::min<std::uint64_t>(remaining_frames, frames_requested));

            for (std::uint32_t frame_index = 0; frame_index < frames_to_write; ++frame_index)
            {
                const std::size_t source_frame = static_cast<std::size_t>(sample_position + frame_index);
                const std::size_t target_frame = static_cast<std::size_t>(frame_index) * out_channel_count_;

                if (buffer.channel_count == 1)
                {
                    const float sample = buffer.samples[source_frame] * gain;
                    target_output[target_frame + 0] += sample;
                    target_output[target_frame + 1] += sample;
                }
                else
                {
                    const std::size_t source_index = source_frame * buffer.channel_count;
                    target_output[target_frame + 0] += buffer.samples[source_index + 0] * gain;
                    target_output[target_frame + 1] += buffer.samples[source_index + 1] * gain;
                }
            }

            sample_position += frames_to_write;
            return frames_to_write;
        };

        switch (node.type)
        {
        case compiler::NodeType::OneShot:
            (void)add_frames(asset_bank_->GetBuffer(asset_ids[0]), voice.sample_position, output, frames);
            return;

        case compiler::NodeType::Random:
            (void)add_frames(asset_bank_->GetBuffer(asset_ids[voice.picked_asset_slot]), voice.sample_position, output, frames);
            return;

        case compiler::NodeType::Loop:
        {
            const assets::DecodedBuffer &buffer = asset_bank_->GetBuffer(asset_ids[0]);
            if (buffer.frame_count == 0)
            {
                std::terminate();
            }

            std::uint32_t written = 0;
            while (written < frames)
            {
                written += add_frames(buffer,
                                      voice.sample_position,
                                      output + static_cast<std::size_t>(written) * out_channel_count_,
                                      frames - written);

                if (written == frames)
                {
                    return;
                }

                if (voice.sample_position != buffer.frame_count)
                {
                    std::terminate();
                }

                if (voice.remaining_loops == 0)
                {
                    std::terminate();
                }

                voice.sample_position = 0;
                if (voice.remaining_loops > 0)
                {
                    --voice.remaining_loops;
                }
            }

            return;
        }

        case compiler::NodeType::Sequence:
        case compiler::NodeType::Select:
        case compiler::NodeType::Blend:
            break;
        }

        std::terminate();
    }

    void AudioRuntime::ActivateVoice(ProgramInstance &instance, const compiler::NodeId leaf_node) noexcept
    {
        const compiler::CompiledNode &node = GetCompiledNode(leaf_node);
        const std::span<const compiler::AssetId> asset_ids = GetNodeAssets(leaf_node);

        for (VoiceState &voice : instance.voices)
        {
            if (voice.active)
            {
                continue;
            }

            voice.leaf_node = leaf_node;
            voice.sample_position = 0;
            voice.picked_asset_slot = 0;
            voice.remaining_loops = 0;
            voice.active = true;

            switch (node.type)
            {
            case compiler::NodeType::OneShot:
                break;

            case compiler::NodeType::Random:
            {
                const std::uint64_t seed = DeriveNodeSeed(instance.instance_id, instance.compiled->id, leaf_node);
                voice.picked_asset_slot = static_cast<std::uint32_t>(seed % asset_ids.size());
                break;
            }

            case compiler::NodeType::Loop:
                if (instance.stop_requested)
                {
                    voice.remaining_loops = 0;
                }
                else if (node.loop_count < 0)
                {
                    voice.remaining_loops = -1;
                }
                else
                {
                    if (node.loop_count == 0)
                    {
                        std::terminate();
                    }

                    voice.remaining_loops = node.loop_count - 1;
                }
                break;

            case compiler::NodeType::Sequence:
            case compiler::NodeType::Select:
            case compiler::NodeType::Blend:
                std::terminate();
            }

            ++instance.active_voice_count;
            for (compiler::NodeId current_node = leaf_node; current_node != kInvalidNodeId; current_node = GetCompiledNode(current_node).parent)
            {
                ++instance.node_state[current_node - instance.compiled->first_node].active_voice_count;
            }

            return;
        }

        std::terminate();
    }

    void AudioRuntime::RetireVoice(ProgramInstance &instance, const std::uint32_t voice_index) noexcept
    {
        VoiceState &voice = instance.voices[voice_index];
        if (!voice.active)
        {
            std::terminate();
        }

        const compiler::NodeId leaf_node = voice.leaf_node;
        voice = VoiceState{};

        --instance.active_voice_count;
        for (compiler::NodeId current_node = leaf_node; current_node != kInvalidNodeId; current_node = GetCompiledNode(current_node).parent)
        {
            --instance.node_state[current_node - instance.compiled->first_node].active_voice_count;
        }

        TryFinishNode(instance, leaf_node);
    }

    void AudioRuntime::EnterNode(ProgramInstance &instance, const compiler::NodeId node_id) noexcept
    {
        NodeRuntimeState &state = instance.node_state[node_id - instance.compiled->first_node];
        if (state.entered)
        {
            std::terminate();
        }

        state.entered = true;
        state.finished = false;
        state.chosen_child = -1;

        const compiler::CompiledNode &node = GetCompiledNode(node_id);
        const std::span<const compiler::NodeId> children = GetNodeChildren(node_id);
        switch (node.type)
        {
        case compiler::NodeType::Sequence:
            if (children.empty())
            {
                std::terminate();
            }

            EnterNode(instance, children[0]);
            return;

        case compiler::NodeType::Select:
        {
            if (children.empty())
            {
                std::terminate();
            }

            const std::uint64_t seed = DeriveNodeSeed(instance.instance_id, instance.compiled->id, node_id);
            const std::int32_t chosen_child = static_cast<std::int32_t>(seed % children.size());
            state.chosen_child = chosen_child;
            EnterNode(instance, children[chosen_child]);
            return;
        }

        case compiler::NodeType::Blend:
            if (children.size() != 2)
            {
                std::terminate();
            }

            EnterNode(instance, children[0]);
            EnterNode(instance, children[1]);
            return;

        case compiler::NodeType::OneShot:
        case compiler::NodeType::Loop:
        case compiler::NodeType::Random:
            ActivateVoice(instance, node_id);
            return;
        }

        std::terminate();
    }

    void AudioRuntime::TryFinishNode(ProgramInstance &instance, const compiler::NodeId node_id) noexcept
    {
        NodeRuntimeState &state = instance.node_state[node_id - instance.compiled->first_node];
        if (!state.entered || state.finished)
        {
            return;
        }

        const compiler::CompiledNode &node = GetCompiledNode(node_id);
        const std::span<const compiler::NodeId> children = GetNodeChildren(node_id);
        switch (node.type)
        {
        case compiler::NodeType::OneShot:
        case compiler::NodeType::Loop:
        case compiler::NodeType::Random:
            if (state.active_voice_count != 0)
            {
                return;
            }
            state.finished = true;
            break;

        case compiler::NodeType::Sequence:
        {
            for (const compiler::NodeId child_id : children)
            {
                NodeRuntimeState &child_state = instance.node_state[child_id - instance.compiled->first_node];
                if (!child_state.entered)
                {
                    if (instance.stop_requested && instance.compiled->stop_mode == compiler::StopMode::Immediate)
                    {
                        break; // skip remaining children, fall through to finish
                    }

                    EnterNode(instance, child_id);
                    return;
                }

                if (!child_state.finished)
                {
                    return;
                }
            }

            state.finished = true;
            break;
        }

        case compiler::NodeType::Select:
        {
            if (state.chosen_child < 0 || state.chosen_child >= static_cast<std::int32_t>(children.size()))
            {
                std::terminate();
            }

            const compiler::NodeId child_id = children[state.chosen_child];
            if (!instance.node_state[child_id - instance.compiled->first_node].finished)
            {
                return;
            }

            state.finished = true;
            break;
        }

        case compiler::NodeType::Blend:
            for (const compiler::NodeId child_id : children)
            {
                if (!instance.node_state[child_id - instance.compiled->first_node].finished)
                {
                    return;
                }
            }

            state.finished = true;
            break;
        }

        const compiler::NodeId parent_id = node.parent;
        if (parent_id != kInvalidNodeId)
        {
            TryFinishNode(instance, parent_id);
        }
    }

    std::uint64_t AudioRuntime::ComputeVoiceTerminalFrames(const ProgramInstance &instance, const VoiceState &voice) const noexcept
    {
        const compiler::CompiledNode &node = GetCompiledNode(voice.leaf_node);
        const std::span<const compiler::AssetId> asset_ids = GetNodeAssets(voice.leaf_node);

        switch (node.type)
        {
        case compiler::NodeType::OneShot:
            return asset_bank_->GetBuffer(asset_ids[0]).frame_count - voice.sample_position;

        case compiler::NodeType::Random:
            return asset_bank_->GetBuffer(asset_ids[voice.picked_asset_slot]).frame_count - voice.sample_position;

        case compiler::NodeType::Loop:
        {
            const assets::DecodedBuffer &buffer = asset_bank_->GetBuffer(asset_ids[0]);
            const std::uint64_t current_pass_remaining = buffer.frame_count - voice.sample_position;
            if (voice.remaining_loops < 0)
            {
                return std::numeric_limits<std::uint64_t>::max();
            }

            return current_pass_remaining + (static_cast<std::uint64_t>(voice.remaining_loops) * buffer.frame_count);
        }

        case compiler::NodeType::Sequence:
        case compiler::NodeType::Select:
        case compiler::NodeType::Blend:
            break;
        }

        std::terminate();
    }

    float AudioRuntime::ComputeVoiceGain(const ProgramInstance &instance, const compiler::NodeId leaf_node) const noexcept
    {
        float gain = instance.volume;

        compiler::NodeId current_node = leaf_node;
        while (current_node != kInvalidNodeId)
        {
            const compiler::CompiledNode &node = GetCompiledNode(current_node);
            gain *= node.authored_gain;

            const compiler::NodeId parent_id = node.parent;
            if (parent_id == kInvalidNodeId)
            {
                break;
            }

            const compiler::CompiledNode &parent = GetCompiledNode(parent_id);
            if (parent.type == compiler::NodeType::Blend)
            {
                if (parent.parameter_slot == kInvalidParameterSlot)
                {
                    std::terminate();
                }

                const std::span<const compiler::NodeId> children = GetNodeChildren(parent_id);
                if (children.size() != 2)
                {
                    std::terminate();
                }

                const float t = std::clamp(instance.parameter_slots[parent.parameter_slot], 0.0f, 1.0f);
                if (current_node == children[0])
                {
                    gain *= (1.0f - t);
                }
                else if (current_node == children[1])
                {
                    gain *= t;
                }
                else
                {
                    std::terminate();
                }
            }

            current_node = parent_id;
        }

        return gain;
    }

    const compiler::CompiledNode &AudioRuntime::GetCompiledNode(const compiler::NodeId node_id) const noexcept
    {
        return compiled_bank_->nodes[node_id];
    }

    std::span<const compiler::NodeId> AudioRuntime::GetNodeChildren(const compiler::NodeId node_id) const noexcept
    {
        return compiled_bank_->GetNodeChildren(GetCompiledNode(node_id));
    }

    std::span<const compiler::AssetId> AudioRuntime::GetNodeAssets(const compiler::NodeId node_id) const noexcept
    {
        return compiled_bank_->GetNodeAssets(GetCompiledNode(node_id));
    }

    std::uint16_t AudioRuntime::FindProgramParameterSlot(const ProgramInstance &instance, const compiler::ParameterId parameter_id) const noexcept
    {
        const std::span<const compiler::ParameterId> parameters = compiled_bank_->GetProgramParameters(instance.compiled->id);
        for (std::uint16_t parameter_index = 0; parameter_index < parameters.size(); ++parameter_index)
        {
            if (parameters[parameter_index] == parameter_id)
            {
                return parameter_index;
            }
        }

        return kInvalidParameterSlot;
    }

    std::size_t AudioRuntime::FindInstanceIndex(const InstanceId instance_id) const noexcept
    {
        for (std::size_t i = 0; i < instances_.size(); ++i)
        {
            if (instances_[i].instance_id == instance_id)
            {
                return i;
            }
        }

        return kNotFound;
    }

    std::uint64_t AudioRuntime::DeriveNodeSeed(const InstanceId instance_id,
                                               const compiler::ProgramId program_id,
                                               const compiler::NodeId node_id) const noexcept
    {
        std::uint64_t seed = root_seed_;
        seed = MixSeed64(seed ^ instance_id);
        seed = MixSeed64(seed ^ program_id);
        seed = MixSeed64(seed ^ node_id);
        return seed;
    }
} // namespace decl_audio::playback
