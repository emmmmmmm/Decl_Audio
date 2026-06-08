#pragma once

#include <algorithm>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../compiler/CompiledBank.hpp"
#include "../core/BankId.hpp"
#include "../playback/AudioCommands.hpp"
#include "WorldState.hpp"

namespace decl_audio::runtime
{
    struct ActiveBehaviorBinding final
    {
        std::string entity_id;
        compiler::BehaviorId behavior_id = 0;
        playback::InstanceId instance_id = 0;
        float volume = 1.0f;
        Vec3 position{};
        std::vector<float> parameter_values;
        std::vector<bool> has_parameter_values;
    };

    class BehaviorResolver final
    {
    public:
        void Reset() noexcept
        {
            active_bindings_.clear();
            candidates_.clear();
            winners_.clear();
            desired_.clear();
            next_instance_id_ = 1;
        }

        template <typename TEmitCommand>
        void Resolve(const WorldState &world_state,
                     const compiler::CompiledBank &compiled_bank,
                     BankId bank_id,
                     TEmitCommand &&emit_command) noexcept
        {
            // Phase 1: Compute the desired set of (entity_id, behavior_id) pairs.
            // For each entity, gather all matching behaviors, filter out those subsumed
            // by a higher-scoring match (matchTags(A) subset of matchTags(B) -> A loses), keep the rest.
            desired_.clear();
            for (const auto &[entity_id, entity_state] : world_state.entities)
            {
                candidates_.clear();
                for (const compiler::CompiledBehavior &behavior : compiled_bank.behaviors)
                {
                    if (MatchesBehavior(entity_state, world_state, behavior, compiled_bank))
                        candidates_.push_back({behavior.id, behavior.score});
                }
                ComputeWinners(compiled_bank, entity_id);
            }

            // Phase 2: Stop or update active bindings.
            std::size_t binding_index = 0;
            while (binding_index < active_bindings_.size())
            {
                ActiveBehaviorBinding &binding = active_bindings_[binding_index];

                if (!IsDesired(binding.entity_id, binding.behavior_id))
                {
                    emit_command(playback::RequestStopCommand{binding.instance_id});
                    active_bindings_[binding_index] = active_bindings_.back();
                    active_bindings_.pop_back();
                    continue;
                }

                // Entity must exist - we only add to desired_ from world_state.entities.
                const EntityState &entity_state = world_state.entities.at(binding.entity_id);

                if (entity_state.HasVolume() && entity_state.GetVolume() != binding.volume)
                {
                    emit_command(playback::SetVolumeCommand{binding.instance_id, entity_state.GetVolume()});
                    binding.volume = entity_state.GetVolume();
                }

                if (entity_state.HasPosition() && entity_state.GetPosition() != binding.position)
                {
                    emit_command(playback::SetPositionCommand{binding.instance_id, entity_state.GetPosition()});
                    binding.position = entity_state.GetPosition();
                }

                const compiler::CompiledBehavior &compiled_behavior = compiled_bank.GetBehavior(binding.behavior_id);
                const compiler::CompiledProgram &compiled_program = compiled_bank.GetProgram(compiled_behavior.program_id);
                const std::span<const compiler::ParameterId> program_parameters = compiled_bank.GetProgramParameters(compiled_program.id);
                for (std::size_t parameter_index = 0; parameter_index < program_parameters.size(); ++parameter_index)
                {
                    const compiler::ParameterId parameter_id = program_parameters[parameter_index];
                    if (!HasFloatValue(entity_state, world_state, parameter_id))
                        continue;

                    const float value = ResolveFloatValue(entity_state, world_state, parameter_id);
                    if (binding.has_parameter_values[parameter_index] && binding.parameter_values[parameter_index] == value)
                        continue;

                    emit_command(playback::SetParameterCommand{binding.instance_id, parameter_id, value});
                    binding.parameter_values[parameter_index] = value;
                    binding.has_parameter_values[parameter_index] = true;
                }

                ++binding_index;
            }

            // Phase 3: Start new desired bindings not yet active.
            for (const auto &[entity_id, behavior_id] : desired_)
            {
                if (FindBindingIndex(entity_id, behavior_id) != kNotFound)
                    continue;

                const EntityState &entity_state = world_state.entities.at(std::string(entity_id));
                const compiler::CompiledBehavior &behavior = compiled_bank.GetBehavior(behavior_id);
                const playback::InstanceId instance_id = MintInstanceId();
                const compiler::CompiledProgram &compiled_program = compiled_bank.GetProgram(behavior.program_id);
                const std::span<const compiler::ParameterId> program_parameters = compiled_bank.GetProgramParameters(compiled_program.id);
                const float initial_volume = entity_state.HasVolume() ? entity_state.GetVolume() : 1.0f;
                const Vec3 initial_position = entity_state.HasPosition() ? entity_state.GetPosition() : Vec3{};

                emit_command(playback::CreateInstanceCommand{instance_id, behavior.program_id, initial_position, initial_volume, bank_id});

                ActiveBehaviorBinding binding{std::string(entity_id), behavior_id, instance_id, initial_volume, initial_position};
                binding.parameter_values.resize(program_parameters.size(), 0.0f);
                binding.has_parameter_values.resize(program_parameters.size(), false);

                for (std::size_t parameter_index = 0; parameter_index < program_parameters.size(); ++parameter_index)
                {
                    const compiler::ParameterId parameter_id = program_parameters[parameter_index];
                    if (!HasFloatValue(entity_state, world_state, parameter_id))
                        continue;

                    const float value = ResolveFloatValue(entity_state, world_state, parameter_id);
                    emit_command(playback::SetParameterCommand{instance_id, parameter_id, value});
                    binding.parameter_values[parameter_index] = value;
                    binding.has_parameter_values[parameter_index] = true;
                }

                active_bindings_.push_back(std::move(binding));
            }
        }

    private:
        static constexpr std::size_t kNotFound = std::numeric_limits<std::size_t>::max();

        struct BehaviorCandidate final
        {
            compiler::BehaviorId behavior_id = 0;
            std::uint32_t score = 0;
        };

        // Returns true if every tag in `a` is also in `b` and `a` is strictly smaller.
        // Used to detect when behavior A is a strict specialization-target of behavior B.
        [[nodiscard]] static bool IsStrictSubset(std::span<const compiler::TagId> a,
                                                 std::span<const compiler::TagId> b) noexcept
        {
            if (a.size() >= b.size())
                return false;
            for (const compiler::TagId tag : a)
            {
                bool found = false;
                for (const compiler::TagId btag : b)
                {
                    if (tag == btag)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return false;
            }
            return true;
        }

        // From the current candidates_ list, determine which behaviors win (are not subsumed
        // by any other match) and append them to desired_.
        void ComputeWinners(const compiler::CompiledBank &compiled_bank, std::string_view entity_id) noexcept
        {
            // Process candidates highest-score first so that when we encounter a lower-scoring
            // candidate we can check it against already-accepted winners only.
            std::sort(candidates_.begin(), candidates_.end(),
                      [](const BehaviorCandidate &a, const BehaviorCandidate &b)
                      { return a.score > b.score; });

            winners_.clear();
            for (const BehaviorCandidate &candidate : candidates_)
            {
                const auto candidate_tags = compiled_bank.GetBehaviorTags(candidate.behavior_id);
                bool subsumed = false;
                for (const compiler::BehaviorId winner_id : winners_)
                {
                    if (IsStrictSubset(candidate_tags, compiled_bank.GetBehaviorTags(winner_id)))
                    {
                        subsumed = true;
                        break;
                    }
                }
                if (!subsumed)
                    winners_.push_back(candidate.behavior_id);
            }

            for (const compiler::BehaviorId behavior_id : winners_)
                desired_.push_back({entity_id, behavior_id});
        }

        [[nodiscard]] bool IsDesired(std::string_view entity_id,
                                     const compiler::BehaviorId behavior_id) const noexcept
        {
            for (const auto &[eid, bid] : desired_)
            {
                if (bid == behavior_id && eid == entity_id)
                    return true;
            }
            return false;
        }

        [[nodiscard]] float ResolveFloatValue(const EntityState &entity_state,
                                              const WorldState &world_state,
                                              const compiler::ParameterId parameter_id) const noexcept
        {
            if (entity_state.HasFloatValue(parameter_id))
                return entity_state.GetFloatValue(parameter_id);
            const auto it = world_state.global_float_values.find(parameter_id);
            return it != world_state.global_float_values.end() ? it->second : 0.0f;
        }

        [[nodiscard]] bool HasFloatValue(const EntityState &entity_state,
                                         const WorldState &world_state,
                                         const compiler::ParameterId parameter_id) const noexcept
        {
            return entity_state.HasFloatValue(parameter_id) || world_state.global_float_values.contains(parameter_id);
        }

        [[nodiscard]] bool MatchesBehavior(const EntityState &entity_state,
                                           const WorldState &world_state,
                                           const compiler::CompiledBehavior &behavior,
                                           const compiler::CompiledBank &compiled_bank) const noexcept
        {
            for (const compiler::TagId tag_id : compiled_bank.GetBehaviorTags(behavior.id))
            {
                if (!entity_state.HasTag(tag_id) && !world_state.global_tags.contains(tag_id))
                    return false;
            }

            for (const compiler::CompiledCondition &condition : compiled_bank.GetBehaviorConditions(behavior.id))
            {
                if (!EvaluateCondition(entity_state, world_state, condition))
                    return false;
            }

            return true;
        }

        [[nodiscard]] bool EvaluateCondition(const EntityState &entity_state,
                                             const WorldState &world_state,
                                             const compiler::CompiledCondition &condition) const noexcept
        {
            const float value = ResolveFloatValue(entity_state, world_state, condition.parameter_id);
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
                    return i;
            }
            return kNotFound;
        }

        [[nodiscard]] playback::InstanceId MintInstanceId() noexcept
        {
            const playback::InstanceId instance_id = next_instance_id_;
            ++next_instance_id_;
            if (next_instance_id_ == 0)
                std::terminate();
            return instance_id;
        }

        std::vector<ActiveBehaviorBinding> active_bindings_;

        // Per-tick scratch buffers -> kept as members to avoid reallocation every frame.
        std::vector<BehaviorCandidate> candidates_;
        std::vector<compiler::BehaviorId> winners_;
        std::vector<std::pair<std::string_view, compiler::BehaviorId>> desired_;

        playback::InstanceId next_instance_id_ = 1;
    };
} // namespace decl_audio::runtime
