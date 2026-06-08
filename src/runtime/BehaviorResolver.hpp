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
    // A read-only view of one loaded bank handed to the resolver each tick. The
    // engine builds these from its bank registry; the resolver never owns banks.
    struct ResolverBankView final
    {
        BankId id;
        const compiler::CompiledBank *compiled = nullptr;
        bool retiring = false; // skip when gathering candidates (section 3.2)
    };

    struct ActiveBehaviorBinding final
    {
        std::string entity_id;
        BankId bank_id{};
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

        // Drop all bindings for a retiring bank without emitting per-instance stops -
        // the RetireBankCommand stops every instance of that bank in one shot on the
        // audio thread (section 3.2). After this, the resolver skips the bank (its
        // view is `retiring`) so it can never re-mint an instance for it.
        void DropBank(const BankId bank_id) noexcept
        {
            std::size_t i = 0;
            while (i < active_bindings_.size())
            {
                if (active_bindings_[i].bank_id == bank_id)
                {
                    active_bindings_[i] = active_bindings_.back();
                    active_bindings_.pop_back();
                    continue;
                }
                ++i;
            }
        }

        template <typename TEmitCommand>
        void Resolve(const WorldState &world_state,
                     std::span<const ResolverBankView> banks,
                     TEmitCommand &&emit_command) noexcept
        {
            // Phase 1: compute the desired set of (entity, bank, behavior) triples.
            // Candidates are gathered from *all* active banks into one list and
            // scored together, so specificity is global: unrelated tag sets layer
            // (both play), a strict superset subsumes (override wins) - across bank
            // boundaries, because vocabulary ids are global after the merge.
            desired_.clear();
            for (const auto &[entity_id, entity_state] : world_state.entities)
            {
                candidates_.clear();
                for (const ResolverBankView &view : banks)
                {
                    if (view.retiring)
                        continue;

                    const compiler::CompiledBank &bank = *view.compiled;
                    for (const compiler::CompiledBehavior &behavior : bank.behaviors)
                    {
                        if (MatchesBehavior(entity_state, world_state, behavior, bank))
                            candidates_.push_back({view.id, behavior.id, behavior.score, view.compiled});
                    }
                }
                ComputeWinners(entity_id);
            }

            // Phase 2: stop or update active bindings.
            std::size_t binding_index = 0;
            while (binding_index < active_bindings_.size())
            {
                ActiveBehaviorBinding &binding = active_bindings_[binding_index];

                if (!IsDesired(binding.entity_id, binding.bank_id, binding.behavior_id))
                {
                    emit_command(playback::RequestStopCommand{binding.instance_id});
                    active_bindings_[binding_index] = active_bindings_.back();
                    active_bindings_.pop_back();
                    continue;
                }

                // Entity exists (desired_ only draws from world_state.entities) and the
                // bank is active (a desired binding came from a non-retiring bank).
                const EntityState &entity_state = world_state.entities.at(binding.entity_id);
                const compiler::CompiledBank &compiled_bank = *FindBank(banks, binding.bank_id);

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

            // Phase 3: start new desired bindings not yet active.
            for (const DesiredBehavior &desired : desired_)
            {
                if (FindBindingIndex(desired.entity_id, desired.bank_id, desired.behavior_id) != kNotFound)
                    continue;

                const EntityState &entity_state = world_state.entities.at(std::string(desired.entity_id));
                const compiler::CompiledBank &compiled_bank = *desired.bank;
                const compiler::CompiledBehavior &behavior = compiled_bank.GetBehavior(desired.behavior_id);
                const playback::InstanceId instance_id = MintInstanceId();
                const compiler::CompiledProgram &compiled_program = compiled_bank.GetProgram(behavior.program_id);
                const std::span<const compiler::ParameterId> program_parameters = compiled_bank.GetProgramParameters(compiled_program.id);
                const float initial_volume = entity_state.HasVolume() ? entity_state.GetVolume() : 1.0f;
                const Vec3 initial_position = entity_state.HasPosition() ? entity_state.GetPosition() : Vec3{};

                emit_command(playback::CreateInstanceCommand{instance_id, behavior.program_id, initial_position, initial_volume, desired.bank_id});

                ActiveBehaviorBinding binding{std::string(desired.entity_id), desired.bank_id, desired.behavior_id, instance_id, initial_volume, initial_position};
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
            BankId bank_id{};
            compiler::BehaviorId behavior_id = 0;
            std::uint32_t score = 0;
            const compiler::CompiledBank *bank = nullptr;
        };

        struct DesiredBehavior final
        {
            std::string_view entity_id;
            BankId bank_id{};
            compiler::BehaviorId behavior_id = 0;
            const compiler::CompiledBank *bank = nullptr;
        };

        [[nodiscard]] static const compiler::CompiledBank *FindBank(std::span<const ResolverBankView> banks, const BankId bank_id) noexcept
        {
            for (const ResolverBankView &view : banks)
            {
                if (view.id == bank_id)
                    return view.compiled;
            }
            return nullptr;
        }

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

        // From the current candidates_ list (gathered across all banks), determine
        // which behaviors win (are not subsumed by any other match) and append them
        // to desired_.
        void ComputeWinners(std::string_view entity_id) noexcept
        {
            // Process candidates highest-score first so that when we encounter a lower-scoring
            // candidate we can check it against already-accepted winners only.
            std::sort(candidates_.begin(), candidates_.end(),
                      [](const BehaviorCandidate &a, const BehaviorCandidate &b)
                      { return a.score > b.score; });

            winners_.clear();
            for (const BehaviorCandidate &candidate : candidates_)
            {
                const auto candidate_tags = candidate.bank->GetBehaviorTags(candidate.behavior_id);
                bool subsumed = false;
                for (const BehaviorCandidate &winner : winners_)
                {
                    if (IsStrictSubset(candidate_tags, winner.bank->GetBehaviorTags(winner.behavior_id)))
                    {
                        subsumed = true;
                        break;
                    }
                }
                if (!subsumed)
                    winners_.push_back(candidate);
            }

            for (const BehaviorCandidate &winner : winners_)
                desired_.push_back({entity_id, winner.bank_id, winner.behavior_id, winner.bank});
        }

        [[nodiscard]] bool IsDesired(std::string_view entity_id,
                                     const BankId bank_id,
                                     const compiler::BehaviorId behavior_id) const noexcept
        {
            for (const DesiredBehavior &desired : desired_)
            {
                if (desired.behavior_id == behavior_id && desired.bank_id == bank_id && desired.entity_id == entity_id)
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
                                                   const BankId bank_id,
                                                   const compiler::BehaviorId behavior_id) const noexcept
        {
            for (std::size_t i = 0; i < active_bindings_.size(); ++i)
            {
                const ActiveBehaviorBinding &binding = active_bindings_[i];
                if (binding.behavior_id == behavior_id && binding.bank_id == bank_id && binding.entity_id == entity_id)
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
        std::vector<BehaviorCandidate> winners_;
        std::vector<DesiredBehavior> desired_;

        playback::InstanceId next_instance_id_ = 1;
    };
} // namespace decl_audio::runtime
