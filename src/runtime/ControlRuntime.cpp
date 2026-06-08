#include "pch.h"

#include "ControlRuntime.hpp"

#include <algorithm>
#include <exception>
#include <variant>

namespace decl_audio::runtime
{
    ControlRuntime::ControlRuntime(VocabularyRegistry &vocabulary, const std::size_t host_queue_capacity)
        : vocabulary_(vocabulary), host_to_control_(host_queue_capacity)
    {
    }

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
        const compiler::TagId tag_id = vocabulary_.GetOrInternTag(command.tag_name);
        EntityState &entity = world_state_.GetOrCreateEntity(command.entity_id);
        const compiler::TagId group_head = vocabulary_.TagGroupHead(tag_id);
        std::erase_if(entity.tags, [&](const compiler::TagId t)
        {
            return vocabulary_.TagGroupHead(t) == group_head;
        });
        entity.tags.insert(tag_id);
    }

    void ControlRuntime::Apply(const SetTransientTagCommand &command) noexcept
    {
        const compiler::TagId tag_id = vocabulary_.GetOrInternTag(command.tag_name);
        world_state_.GetOrCreateEntity(command.entity_id).transient_tags.insert(tag_id);
        transientTags_.push_back({command.entity_id, tag_id});
    }

    void ControlRuntime::Apply(const RemoveTagCommand &command) noexcept
    {
        const auto entity_it = world_state_.entities.find(command.entity_id);
        if (entity_it == world_state_.entities.end())
        {
            return;
        }

        entity_it->second.tags.erase(vocabulary_.GetOrInternTag(command.tag_name));
    }

    void ControlRuntime::Apply(const SetFloatValueCommand &command) noexcept
    {
        const compiler::ParameterId parameter_id = vocabulary_.GetOrInternParam(command.parameter_name);
        world_state_.GetOrCreateEntity(command.entity_id).float_values[parameter_id] = command.value;
    }

    void ControlRuntime::Apply(const SetGlobalTagCommand &command) noexcept
    {
        const compiler::TagId tag_id = vocabulary_.GetOrInternTag(command.tag_name);
        const compiler::TagId group_head = vocabulary_.TagGroupHead(tag_id);
        std::erase_if(world_state_.global_tags, [&](const compiler::TagId t)
        {
            return vocabulary_.TagGroupHead(t) == group_head;
        });
        world_state_.global_tags.insert(tag_id);
    }
    void ControlRuntime::Apply(const RemoveGlobalTagCommand &command) noexcept
    {
        world_state_.global_tags.erase(vocabulary_.GetOrInternTag(command.tag_name));
    }
    void ControlRuntime::Apply(const SetGlobalFloatValueCommand &command) noexcept
    {
        const compiler::ParameterId parameter_id = vocabulary_.GetOrInternParam(command.parameter_name);
        world_state_.global_float_values[parameter_id] = command.value; // uuuuuh do we need to ... init those?
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

    void ControlRuntime::Apply(const SetMasterGainCommand &command) noexcept
    {
        master_gain_ = command.gain;
        master_gain_dirty_ = true;
    }

    void ControlRuntime::Apply(const UnloadBankCommand &command) noexcept
    {
        // The engine owns the bank registry, so just record the request; it drains
        // these after Tick() and runs the retire handshake.
        pending_unloads_.push_back(command.bank_path);
    }
} // namespace decl_audio::runtime
