#pragma once

#include <cstddef>

#include "../RingBuffer.hpp"
#include "HostCommands.hpp"
#include "WorldState.hpp"

namespace decl_audio::runtime
{
    class ControlRuntime final
    {
    public:
        static constexpr std::size_t HostQueueCapacity = 1024;

        void Submit(HostCommand command);
        void Tick() noexcept;

        [[nodiscard]] const WorldState &GetWorldState() const noexcept
        {
            return world_state_;
        }

    private:
        void Apply(const SetTagCommand &command) noexcept;
        void Apply(const RemoveTagCommand &command) noexcept;
        void Apply(const SetFloatValueCommand &command) noexcept;
        void Apply(const SetEntityVolumeCommand &command) noexcept;
        void Apply(const SetEntityPositionCommand &command) noexcept;
        void Apply(const DestroyEntityCommand &command) noexcept;

        RingBuffer<HostCommand, HostQueueCapacity> host_to_control_;
        WorldState world_state_;
    };
} // namespace decl_audio::runtime
