#include "pch.h"

#include "ControlRuntime.hpp"

#include <exception>
#include <variant>

namespace decl_audio::runtime
{
    void ControlRuntime::Submit(HostCommand command)
    {
        if (!host_to_control_.push(command))
        {
            std::terminate();
        }
    }

    void ControlRuntime::Tick() noexcept
    {
        HostCommand command;
        while (host_to_control_.pop(command))
        {
            std::visit(
                [this](const auto &typed_command)
                {
                    Apply(typed_command);
                },
                command);
        }
    }

    void ControlRuntime::Apply(const SetTagCommand &command) noexcept
    {
        world_state_.GetOrCreateEntity(command.entity_id).tags.insert(command.tag_id);
    }

    void ControlRuntime::Apply(const RemoveTagCommand &command) noexcept
    {
        const auto entity_it = world_state_.entities.find(command.entity_id);
        if (entity_it == world_state_.entities.end())
        {
            return;
        }

        entity_it->second.tags.erase(command.tag_id);
    }

    void ControlRuntime::Apply(const SetFloatValueCommand &command) noexcept
    {
        world_state_.GetOrCreateEntity(command.entity_id).float_values[command.parameter_id] = command.value;
    }

    void ControlRuntime::Apply(const DestroyEntityCommand &command) noexcept
    {
        world_state_.entities.erase(command.entity_id);
    }
} // namespace decl_audio::runtime
