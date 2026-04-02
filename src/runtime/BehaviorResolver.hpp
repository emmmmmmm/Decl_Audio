#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "../compiler/CompiledBank.hpp"
#include "../playback/AudioCommands.hpp"
#include "WorldState.hpp"

namespace decl_audio::runtime
{
    struct ActiveBehaviorBinding final
    {
        std::string entity_id;
        compiler::BehaviorId behavior_id = 0;
        playback::InstanceId instance_id = 0;
    };

    class BehaviorResolver final
    {
    public:
        void Reset() noexcept
        {
            active_bindings_.clear();
            next_instance_id_ = 1;
        }

        template <typename TEmitCommand>
        void Resolve(const WorldState &world_state,
                     const compiler::CompiledBank &compiled_bank,
                     TEmitCommand &&emit_command) noexcept
        {
            std::size_t binding_index = 0;
            while (binding_index < active_bindings_.size())
            {
                const ActiveBehaviorBinding &binding = active_bindings_[binding_index];
                const auto entity_it = world_state.entities.find(binding.entity_id);
                if (entity_it == world_state.entities.end() ||
                    !MatchesBehavior(entity_it->second, compiled_bank.GetBehavior(binding.behavior_id), compiled_bank))
                {
                    emit_command(playback::RequestStopCommand{binding.instance_id});
                    active_bindings_[binding_index] = active_bindings_.back();
                    active_bindings_.pop_back();
                    continue;
                }

                ++binding_index;
            }

            for (const auto &[entity_id, entity_state] : world_state.entities)
            {
                for (const compiler::CompiledBehavior &behavior : compiled_bank.behaviors)
                {
                    if (!MatchesBehavior(entity_state, behavior, compiled_bank))
                    {
                        continue;
                    }

                    if (FindBindingIndex(entity_id, behavior.id) != kNotFound)
                    {
                        continue;
                    }

                    const playback::InstanceId instance_id = MintInstanceId();
                    emit_command(playback::CreateInstanceCommand{
                        instance_id,
                        behavior.program_id,
                        Vec3{},
                        1.0f});
                    active_bindings_.push_back(ActiveBehaviorBinding{
                        entity_id,
                        behavior.id,
                        instance_id});
                }
            }
        }

    private:
        static constexpr std::size_t kNotFound = std::numeric_limits<std::size_t>::max();

        [[nodiscard]] bool MatchesBehavior(const EntityState &entity_state,
                                           const compiler::CompiledBehavior &behavior,
                                           const compiler::CompiledBank &compiled_bank) const noexcept
        {
            for (const compiler::TagId tag_id : compiled_bank.GetBehaviorTags(behavior.id))
            {
                if (!entity_state.HasTag(tag_id))
                {
                    return false;
                }
            }

            for (const compiler::CompiledCondition &condition : compiled_bank.GetBehaviorConditions(behavior.id))
            {
                if (!EvaluateCondition(entity_state, condition))
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] bool EvaluateCondition(const EntityState &entity_state,
                                             const compiler::CompiledCondition &condition) const noexcept
        {
            const float value = entity_state.GetFloatValue(condition.parameter_id);
            switch (condition.op)
            {
            case compiler::ComparisonOp::Less:
                return value < condition.literal;

            case compiler::ComparisonOp::LessOrEqual:
                return value <= condition.literal;

            case compiler::ComparisonOp::Equal:
                return value == condition.literal;

            case compiler::ComparisonOp::GreaterOrEqual:
                return value >= condition.literal;

            case compiler::ComparisonOp::Greater:
                return value > condition.literal;
            }

            std::terminate();
        }

        [[nodiscard]] std::size_t FindBindingIndex(const std::string_view entity_id,
                                                   const compiler::BehaviorId behavior_id) const noexcept
        {
            for (std::size_t i = 0; i < active_bindings_.size(); ++i)
            {
                const ActiveBehaviorBinding &binding = active_bindings_[i];
                if (binding.behavior_id == behavior_id && binding.entity_id == entity_id)
                {
                    return i;
                }
            }

            return kNotFound;
        }

        [[nodiscard]] playback::InstanceId MintInstanceId() noexcept
        {
            const playback::InstanceId instance_id = next_instance_id_;
            ++next_instance_id_;
            if (next_instance_id_ == 0)
            {
                std::terminate();
            }

            return instance_id;
        }

        std::vector<ActiveBehaviorBinding> active_bindings_;
        playback::InstanceId next_instance_id_ = 1;
    };
} // namespace decl_audio::runtime
