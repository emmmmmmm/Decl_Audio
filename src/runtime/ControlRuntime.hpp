#pragma once

#include <cstddef>

#include "../core/RingBuffer.hpp"
#include "HostCommands.hpp"
#include "WorldState.hpp"
#include <vector>
#include <tuple>

namespace decl_audio::compiler { struct CompiledBank; }

namespace decl_audio::runtime
{
    class ControlRuntime final
    {
    public:
        explicit ControlRuntime(std::size_t host_queue_capacity = 1024);

        void Submit(HostCommand command);
        void Tick() noexcept;

        void ClearTransientTags();

        [[nodiscard]] const WorldState &GetWorldState() const noexcept
        {
            return world_state_;
        }

        void SetBank(const compiler::CompiledBank *bank) noexcept
        {
            compiled_bank_ = bank;
        }

        [[nodiscard]] bool ListenerPositionChanged(Vec3 &position) noexcept
        {
            if (!listener_position_dirty_)
            {
                return false;
            }

            position = listener_position_;
            listener_position_dirty_ = false;
            return true;
        }

        [[nodiscard]] bool MasterGainChanged(float &gain) noexcept
        {
            if (!master_gain_dirty_)
            {
                return false;
            }

            gain = master_gain_;
            master_gain_dirty_ = false;
            return true;
        }

    private:
        void Apply(const SetTagCommand &command) noexcept;
        void Apply(const SetTransientTagCommand &command) noexcept;
        void Apply(const RemoveTagCommand &command) noexcept;
        void Apply(const SetFloatValueCommand &command) noexcept;
        void Apply(const SetGlobalTagCommand &command) noexcept;
        void Apply(const RemoveGlobalTagCommand &command) noexcept;
        void Apply(const SetGlobalFloatValueCommand &command) noexcept;
        void Apply(const SetEntityVolumeCommand &command) noexcept;
        void Apply(const SetEntityPositionCommand &command) noexcept;
        void Apply(const SetListenerPositionCommand &command) noexcept;
        void Apply(const DestroyEntityCommand &command) noexcept;
        void Apply(const SetMasterGainCommand &command) noexcept;

        const compiler::CompiledBank *compiled_bank_ = nullptr;
        RingBuffer<HostCommand> host_to_control_;
        WorldState world_state_;
        Vec3 listener_position_{};
        bool listener_position_dirty_ = false;
        float master_gain_ = 1.0f;
        bool master_gain_dirty_ = false;

        std::vector<std::tuple<std::string, decl_audio::compiler::TagId>> transientTags_{};
    };
} // namespace decl_audio::runtime
