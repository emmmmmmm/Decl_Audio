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

    void ControlRuntime::ClearTransientTags()
    {
        for (auto [entity, tag] : transientTags_)
        {
            const auto entity_it = world_state_.entities.find(entity);
            if (entity_it == world_state_.entities.end())
                continue;
            entity_it->second.transient_tags.erase(tag);
        }
        transientTags_.clear();
    }

    void ControlRuntime::Apply(const SetTagCommand &command) noexcept
    {
        world_state_.GetOrCreateEntity(command.entity_id).tags.insert(command.tag_id);
    }

    void ControlRuntime::Apply(const SetTransientTagCommand &command) noexcept
    {
        world_state_.GetOrCreateEntity(command.entity_id).transient_tags.insert(command.tag_id);
        transientTags_.push_back({command.entity_id, command.tag_id});
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

    void ControlRuntime::Apply(const SetEntityVolumeCommand &command) noexcept
    {
        EntityState &entity = world_state_.GetOrCreateEntity(command.entity_id);
        entity.volume = command.volume;
        entity.has_volume = true;
    }

    void ControlRuntime::Apply(const SetEntityPositionCommand &command) noexcept
    {
        EntityState &entity = world_state_.GetOrCreateEntity(command.entity_id);
        entity.position = command.position;
        entity.has_position = true;
    }

    void ControlRuntime::Apply(const SetListenerPositionCommand &command) noexcept
    {
        listener_position_ = command.position;
        listener_position_dirty_ = true;
    }

    void ControlRuntime::Apply(const DestroyEntityCommand &command) noexcept
    {
        world_state_.entities.erase(command.entity_id);
    }
} // namespace decl_audio::runtime
