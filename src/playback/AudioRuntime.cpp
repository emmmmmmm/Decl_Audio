#include "pch.h"

#include "AudioRuntime.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <variant>

namespace decl_audio::playback
{
    namespace
    {
        constexpr std::size_t kNotFound = std::numeric_limits<std::size_t>::max();

        // MixSeed64 finalizer used as a deterministic 64-bit seed mixer.
        // We use this to decorrelate stable ids before modulo-picking random assets.
        [[nodiscard]] std::uint64_t MixSeed64(std::uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ULL;
            value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
            value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
            return value ^ (value >> 31U);
        }

        template <typename TState>
        [[nodiscard]] TState &RequireState(ActiveContainerState &state) noexcept
        {
            TState *typed_state = std::get_if<TState>(&state);
            if (typed_state == nullptr)
            {
                std::terminate();
            }

            return *typed_state;
        }

        template <typename TState>
        [[nodiscard]] const TState &RequireState(const ActiveContainerState &state) noexcept
        {
            const TState *typed_state = std::get_if<TState>(&state);
            if (typed_state == nullptr)
            {
                std::terminate();
            }

            return *typed_state;
        }
    } // namespace

    AudioRuntime::AudioRuntime(const std::uint64_t root_seed,
                               const std::size_t max_instances,
                               const std::uint32_t max_block_frames)
        : root_seed_(root_seed),
          max_instances_(max_instances),
          max_block_frames_(max_block_frames)
    {
        instances_.reserve(max_instances_);
        scratch_.resize(static_cast<std::size_t>(max_block_frames_) * OutputChannelCount);
    }

    void AudioRuntime::SetBanks(const compiler::CompiledBank *compiled_bank, const assets::AssetBank *asset_bank) noexcept
    {
        Clear();
        compiled_bank_ = compiled_bank;
        asset_bank_ = asset_bank;
    }

    void AudioRuntime::Clear() noexcept
    {
        instances_.clear();

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

        std::fill_n(output, static_cast<std::size_t>(frames) * OutputChannelCount, 0.0f);

        ApplyPendingCommands();

        if (instances_.empty())
        {
            return;
        }

        std::size_t instance_index = 0;
        while (instance_index < instances_.size())
        {
            ProgramInstance &instance = instances_[instance_index];
            std::fill_n(scratch_.data(), static_cast<std::size_t>(frames) * OutputChannelCount, 0.0f);

            const std::uint32_t written = RenderProgramInstance(instance, scratch_.data(), frames);
            const float instance_volume = instance.volume;
            for (std::size_t sample_index = 0; sample_index < static_cast<std::size_t>(frames) * OutputChannelCount; ++sample_index)
            {
                output[sample_index] += scratch_[sample_index] * instance_volume;
            }

            if (written < frames)
            {
                instances_[instance_index] = instances_.back();
                instances_.pop_back();
                continue;
            }

            ++instance_index;
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
        const compiler::CompiledContainer &container = GetCompiledContainer(instance);
        snapshot.instance_id = instance.instance_id;
        snapshot.program_id = instance.compiled->id;
        snapshot.cursor = instance.cursor;
        snapshot.container_type = container.type;
        snapshot.volume = instance.volume;
        snapshot.position = instance.position;
        snapshot.stop_requested = instance.stop_requested;
        return true;
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

        ProgramInstance instance;
        instance.instance_id = command.instance_id;
        instance.compiled = &compiled_program;
        instance.cursor = 0;
        instance.volume = command.volume;
        instance.position = command.position;
        instance.stop_requested = false;
        instance.current = MakeContainerState(instance, 0);

        instances_.push_back(instance);
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

    void AudioRuntime::Apply(const RequestStopCommand &command) noexcept
    {
        const std::size_t instance_index = FindInstanceIndex(command.instance_id);
        if (instance_index == kNotFound)
        {
            return;
        }

        ProgramInstance &instance = instances_[instance_index];
        instance.stop_requested = true;

        if (GetCompiledContainer(instance).type == compiler::ContainerType::Loop)
        {
            RequireState<LoopState>(instance.current).remaining_loops = 0;
        }
    }

    std::uint32_t AudioRuntime::RenderProgramInstance(ProgramInstance &instance, float *output, const std::uint32_t frames) noexcept
    {
        std::uint32_t written = 0;
        while (written < frames)
        {
            const std::uint32_t container_written = RenderCurrentContainer(instance, output + static_cast<std::size_t>(written) * OutputChannelCount, frames - written);
            written += container_written;

            if (written < frames)
            {
                ++instance.cursor;
                if (instance.cursor >= instance.compiled->container_count)
                {
                    break;
                }

                instance.current = MakeContainerState(instance, instance.cursor);
            }
        }

        return written;
    }

    std::uint32_t AudioRuntime::RenderCurrentContainer(ProgramInstance &instance, float *output, const std::uint32_t frames) noexcept
    {
        const compiler::CompiledContainer &container = GetCompiledContainer(instance);
        const std::span<const compiler::AssetId> asset_ids = compiled_bank_->GetContainerAssets(container);

        auto write_frames = [&](const assets::DecodedBuffer &buffer,
                                std::uint64_t &sample_position,
                                float *target_output,
                                const std::uint32_t frames_requested) noexcept -> std::uint32_t
        {
            const std::uint64_t remaining_frames = buffer.frame_count - sample_position;
            const std::uint32_t frames_to_write = static_cast<std::uint32_t>(std::min<std::uint64_t>(remaining_frames, frames_requested));

            for (std::uint32_t frame_index = 0; frame_index < frames_to_write; ++frame_index)
            {
                const std::size_t source_frame = static_cast<std::size_t>(sample_position + frame_index);
                const std::size_t target_frame = static_cast<std::size_t>(frame_index) * OutputChannelCount;

                if (buffer.channel_count == 1)
                {
                    const float sample = buffer.samples[source_frame] * container.volume;
                    target_output[target_frame + 0] = sample;
                    target_output[target_frame + 1] = sample;
                }
                else
                {
                    const std::size_t source_index = source_frame * buffer.channel_count;
                    target_output[target_frame + 0] = buffer.samples[source_index + 0] * container.volume;
                    target_output[target_frame + 1] = buffer.samples[source_index + 1] * container.volume;
                }
            }

            sample_position += frames_to_write;
            return frames_to_write;
        };

        switch (container.type)
        {
        case compiler::ContainerType::OneShot:
        {
            OneShotState &state = RequireState<OneShotState>(instance.current);
            return write_frames(asset_bank_->GetBuffer(asset_ids[0]), state.sample_position, output, frames);
        }

        case compiler::ContainerType::Random:
        {
            RandomState &state = RequireState<RandomState>(instance.current);
            return write_frames(asset_bank_->GetBuffer(asset_ids[state.picked_asset_slot]), state.sample_position, output, frames);
        }

        case compiler::ContainerType::Loop:
        {
            LoopState &state = RequireState<LoopState>(instance.current);
            const assets::DecodedBuffer &buffer = asset_bank_->GetBuffer(asset_ids[0]);
            if (buffer.frame_count == 0)
            {
                std::terminate();
            }

            std::uint32_t written = 0;
            while (written < frames)
            {
                const std::uint32_t pass_written = write_frames(buffer,
                                                                state.sample_position,
                                                                output + static_cast<std::size_t>(written) * OutputChannelCount,
                                                                frames - written);
                written += pass_written;

                if (written == frames)
                {
                    break;
                }

                if (state.sample_position != buffer.frame_count)
                {
                    break;
                }

                if (state.remaining_loops == 0)
                {
                    break;
                }

                state.sample_position = 0;
                if (state.remaining_loops > 0)
                {
                    --state.remaining_loops;
                }
            }

            return written;
        }
        }

        std::terminate();
    }

    ActiveContainerState AudioRuntime::MakeContainerState(const ProgramInstance &instance, const std::uint32_t cursor) const noexcept
    {
        const compiler::CompiledContainer &container = GetCompiledContainer(instance, cursor);

        switch (container.type)
        {
        case compiler::ContainerType::OneShot:
            return OneShotState{};

        case compiler::ContainerType::Loop:
        {
            LoopState state;
            state.sample_position = 0;
            if (instance.stop_requested)
            {
                state.remaining_loops = 0;
            }
            else if (container.loop_count < 0)
            {
                state.remaining_loops = -1;
            }
            else
            {
                if (container.loop_count == 0)
                {
                    std::terminate();
                }

                state.remaining_loops = container.loop_count - 1;
            }

            return state;
        }

        case compiler::ContainerType::Random:
        {
            RandomState state;
            const std::uint64_t seed = DeriveContainerSeed(instance.instance_id, instance.compiled->id, cursor);
            state.picked_asset_slot = static_cast<std::uint32_t>(seed % container.asset_count);
            state.sample_position = 0;
            return state;
        }
        }

        std::terminate();
    }

    const compiler::CompiledContainer &AudioRuntime::GetCompiledContainer(const ProgramInstance &instance) const noexcept
    {
        return GetCompiledContainer(instance, instance.cursor);
    }

    const compiler::CompiledContainer &AudioRuntime::GetCompiledContainer(const ProgramInstance &instance, const std::uint32_t cursor) const noexcept
    {
        return compiled_bank_->containers[instance.compiled->first_container + cursor];
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

    std::uint64_t AudioRuntime::DeriveContainerSeed(const InstanceId instance_id, const compiler::ProgramId program_id, const std::uint32_t cursor) const noexcept
    {
        std::uint64_t seed = MixSeed64(root_seed_);
        seed ^= MixSeed64(instance_id);
        seed ^= MixSeed64(static_cast<std::uint64_t>(program_id));
        seed ^= MixSeed64(static_cast<std::uint64_t>(cursor));
        return MixSeed64(seed);
    }
} // namespace decl_audio::playback
