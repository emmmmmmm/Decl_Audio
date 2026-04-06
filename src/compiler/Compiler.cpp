#include "pch.h"

#include "Compiler.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace decl_audio::compiler
{
    namespace
    {
        template <typename IdType>
        [[nodiscard]] IdType InternName(std::unordered_map<std::string, IdType> &name_to_id, const std::string &name)
        {
            const auto [it, inserted] = name_to_id.emplace(name, static_cast<IdType>(name_to_id.size()));
            (void)inserted;
            return it->second;
        }

        [[nodiscard]] AssetId InternAsset(CompiledBank &bank, const std::string &asset_name)
        {
            auto it = bank.asset_name_to_id.find(asset_name);
            if (it != bank.asset_name_to_id.end())
                return it->second;

            const AssetId id = static_cast<AssetId>(bank.asset_paths.size());
            bank.asset_name_to_id.emplace(asset_name, id);
            bank.asset_paths.push_back(asset_name);
            return id;
        }

        struct ProgramLoweringContext final
        {
            CompiledBank &bank;
            std::string_view behavior_id;
            std::vector<decl_audio::Diagnostic> &diagnostics;
            const std::unordered_set<ParameterId> &declared_parameter_ids;
            std::unordered_map<ParameterId, std::uint16_t> parameter_slots;
            std::vector<ParameterId> program_parameters;

            void Error(const decl_audio::SourceLocation &location, std::string_view msg)
            {
                diagnostics.push_back(MakeError(location,
                                                "behavior '" + std::string(behavior_id) + "' " + std::string(msg)));
            }
        };

        [[nodiscard]] std::uint16_t RequireProgramParameterSlot(ProgramLoweringContext &context,
                                                                const AuthoringNode &authoring_node) noexcept
        {
            if (authoring_node.parameter.empty())
            {
                context.Error(authoring_node.location, "blend nodes require a parameter");
                return kInvalidParameterSlot;
            }

            const auto parameter_it = context.bank.parameter_name_to_id.find(authoring_node.parameter);
            if (parameter_it == context.bank.parameter_name_to_id.end() ||
                !context.declared_parameter_ids.contains(parameter_it->second))
            {
                context.Error(authoring_node.location,
                              "blend node parameter '" + authoring_node.parameter + "' must be declared in behavior.parameters");
                return kInvalidParameterSlot;
            }

            const ParameterId parameter_id = parameter_it->second;
            const auto slot_it = context.parameter_slots.find(parameter_id);
            if (slot_it != context.parameter_slots.end())
                return slot_it->second;

            const std::uint16_t slot = static_cast<std::uint16_t>(context.program_parameters.size());
            context.parameter_slots.emplace(parameter_id, slot);
            context.program_parameters.push_back(parameter_id);
            return slot;
        }

        void ValidateLeafShape(const AuthoringNode &authoring_node,
                               ProgramLoweringContext &context,
                               std::string_view leaf_name)
        {
            if (!authoring_node.children.empty())
                context.Error(authoring_node.location, std::string(leaf_name) + " nodes do not allow children");

            if (!authoring_node.parameter.empty())
                context.Error(authoring_node.location, std::string(leaf_name) + " nodes do not allow parameters");
        }

        void ValidateStructuralShape(const AuthoringNode &authoring_node,
                                     ProgramLoweringContext &context,
                                     std::string_view node_name,
                                     bool parameter_allowed)
        {
            if (!authoring_node.assets.empty())
                context.Error(authoring_node.location, std::string(node_name) + " nodes do not allow assets");

            if (!parameter_allowed && !authoring_node.parameter.empty())
                context.Error(authoring_node.location, std::string(node_name) + " nodes do not allow parameters");
        }

        [[nodiscard]] std::uint32_t LowerNode(ProgramLoweringContext &context,
                                              const AuthoringNode &authoring_node,
                                              NodeId parent_id)
        {
            const NodeId node_id = static_cast<NodeId>(context.bank.nodes.size());
            context.bank.nodes.push_back(CompiledNode{});

            context.bank.nodes[node_id].parent = parent_id;
            context.bank.nodes[node_id].authored_gain = authoring_node.volume;
            context.bank.nodes[node_id].first_child = 0;
            context.bank.nodes[node_id].first_asset = 0;
            context.bank.nodes[node_id].loop_count = authoring_node.loop_count;
            context.bank.nodes[node_id].parameter_slot = kInvalidParameterSlot;

            std::uint32_t max_concurrent_voices = 0;
            std::vector<NodeId> child_ids;

            // Helpers: lower all children, accumulating max or sum concurrency.
            auto LowerChildrenMax = [&]()
            {
                for (const AuthoringNode &child : authoring_node.children)
                {
                    child_ids.push_back(static_cast<NodeId>(context.bank.nodes.size()));
                    max_concurrent_voices = std::max(max_concurrent_voices, LowerNode(context, child, node_id));
                }
            };
            auto LowerChildrenSum = [&]()
            {
                for (const AuthoringNode &child : authoring_node.children)
                {
                    child_ids.push_back(static_cast<NodeId>(context.bank.nodes.size()));
                    max_concurrent_voices += LowerNode(context, child, node_id);
                }
            };
            // Intern all assets in authoring_node.assets onto the node (leaf nodes only).
            auto InternNodeAssets = [&]()
            {
                context.bank.nodes[node_id].first_asset = static_cast<std::uint32_t>(context.bank.node_assets.size());
                for (const std::string &asset_name : authoring_node.assets)
                    context.bank.node_assets.push_back(InternAsset(context.bank, asset_name));
                max_concurrent_voices = 1;
            };

            switch (authoring_node.type)
            {
            case NodeType::Sequence:
                context.bank.nodes[node_id].type = NodeType::Sequence;
                ValidateStructuralShape(authoring_node, context, "sequence", false);
                if (authoring_node.children.empty())
                    context.Error(authoring_node.location, "has an empty sequence node");
                LowerChildrenMax();
                break;

            case NodeType::Select:
                context.bank.nodes[node_id].type = NodeType::Select;
                ValidateStructuralShape(authoring_node, context, "select", false);
                if (authoring_node.children.empty())
                    context.Error(authoring_node.location, "select nodes require at least one child");
                LowerChildrenMax();
                break;

            case NodeType::Blend:
                context.bank.nodes[node_id].type = NodeType::Blend;
                ValidateStructuralShape(authoring_node, context, "blend", true);
                context.bank.nodes[node_id].parameter_slot = RequireProgramParameterSlot(context, authoring_node);
                if (authoring_node.children.size() != 2)
                    context.Error(authoring_node.location, "blend nodes require exactly two children");
                LowerChildrenSum();
                break;

            case NodeType::OneShot:
                context.bank.nodes[node_id].type = NodeType::OneShot;
                ValidateLeafShape(authoring_node, context, "oneshot");
                if (authoring_node.assets.size() != 1)
                    context.Error(authoring_node.location, "oneshot nodes require exactly one asset");
                InternNodeAssets();
                break;

            case NodeType::Loop:
                context.bank.nodes[node_id].type = NodeType::Loop;
                ValidateLeafShape(authoring_node, context, "loop");
                if (authoring_node.assets.size() != 1)
                    context.Error(authoring_node.location, "loop nodes require exactly one asset");
                if (authoring_node.loop_count == 0)
                    context.Error(authoring_node.location, "loop nodes do not allow loopCount = 0");
                InternNodeAssets();
                break;

            case NodeType::Random:
                context.bank.nodes[node_id].type = NodeType::Random;
                ValidateLeafShape(authoring_node, context, "random");
                if (authoring_node.assets.empty())
                    context.Error(authoring_node.location, "random nodes require at least one asset");
                InternNodeAssets();
                break;
            }

            if (!child_ids.empty())
            {
                context.bank.nodes[node_id].first_child = static_cast<std::uint32_t>(context.bank.node_children.size());
                context.bank.node_children.insert(context.bank.node_children.end(), child_ids.begin(), child_ids.end());
            }
            context.bank.nodes[node_id].child_count = static_cast<std::uint32_t>(child_ids.size());

            const bool is_leaf = authoring_node.type == NodeType::OneShot ||
                                 authoring_node.type == NodeType::Loop ||
                                 authoring_node.type == NodeType::Random;
            if (is_leaf)
                context.bank.nodes[node_id].asset_count = static_cast<std::uint32_t>(context.bank.node_assets.size() - context.bank.nodes[node_id].first_asset);
            else
            {
                context.bank.nodes[node_id].first_asset = 0;
                context.bank.nodes[node_id].asset_count = 0;
            }

            return max_concurrent_voices;
        }
    } // namespace

    CompileResult CompileAuthoringDocument(const AuthoringDocument &document)
    {
        CompileResult result;

        for (const AuthoringBehavior &behavior : document.behaviors)
        {
            if (behavior.id.empty())
            {
                result.diagnostics.push_back(MakeError(behavior.location, "behavior id must not be empty"));
                continue;
            }

            const BehaviorId behavior_id = static_cast<BehaviorId>(result.bank.behaviors.size());
            const auto [behavior_it, behavior_inserted] = result.bank.behavior_name_to_id.emplace(behavior.id, behavior_id);
            if (!behavior_inserted)
                result.diagnostics.push_back(MakeError(behavior.location, "duplicate behavior id '" + behavior.id + "'"));

            std::unordered_set<ParameterId> declared_parameter_ids;
            for (const std::string &parameter_name : behavior.parameters)
            {
                if (parameter_name.empty())
                {
                    result.diagnostics.push_back(MakeError(behavior.location, "behavior '" + behavior.id + "' declares an empty parameter name"));
                    continue;
                }

                if (parameter_name == "volume" || parameter_name == "position")
                {
                    result.diagnostics.push_back(MakeError(behavior.location, "behavior '" + behavior.id + "' must not declare reserved runtime parameter '" + parameter_name + "'"));
                    continue;
                }

                const ParameterId parameter_id = InternName(result.bank.parameter_name_to_id, parameter_name);
                declared_parameter_ids.insert(parameter_id);
            }

            const std::uint32_t first_tag = static_cast<std::uint32_t>(result.bank.behavior_tags.size());
            for (const std::string &tag_name : behavior.match_tags)
                result.bank.behavior_tags.push_back(InternName(result.bank.tag_name_to_id, tag_name));

            const std::uint32_t first_condition = static_cast<std::uint32_t>(result.bank.conditions.size());
            for (const AuthoringCondition &condition : behavior.match_conditions)
            {
                if (condition.parameter.empty())
                {
                    result.diagnostics.push_back(MakeError(condition.location, "behavior '" + behavior.id + "' has a condition with an empty parameter name"));
                    continue;
                }

                if (condition.parameter == "volume" || condition.parameter == "position")
                {
                    result.diagnostics.push_back(MakeError(condition.location, "behavior '" + behavior.id + "' must not use reserved runtime parameter '" + condition.parameter + "' in match conditions"));
                    continue;
                }

                CompiledCondition compiled_condition;
                compiled_condition.parameter_id = InternName(result.bank.parameter_name_to_id, condition.parameter);
                compiled_condition.op = condition.op;
                compiled_condition.literal = condition.literal;
                result.bank.conditions.push_back(compiled_condition);
            }

            const ProgramId program_id = static_cast<ProgramId>(result.bank.programs.size());
            result.bank.program_name_to_id.emplace(behavior.id, program_id);

            const std::uint32_t first_node = static_cast<std::uint32_t>(result.bank.nodes.size());
            const NodeId root_node = static_cast<NodeId>(result.bank.nodes.size());
            result.bank.nodes.push_back(CompiledNode{});

            ProgramLoweringContext context{
                result.bank,
                behavior.id,
                result.diagnostics,
                declared_parameter_ids};

            result.bank.nodes[root_node].type = NodeType::Sequence;
            result.bank.nodes[root_node].parent = kInvalidNodeId;
            result.bank.nodes[root_node].authored_gain = 1.0f;
            result.bank.nodes[root_node].first_child = 0;
            result.bank.nodes[root_node].first_asset = static_cast<std::uint32_t>(result.bank.node_assets.size());
            result.bank.nodes[root_node].parameter_slot = kInvalidParameterSlot;

            std::uint32_t max_concurrent_voices = 0;
            std::vector<NodeId> root_child_ids;
            for (const AuthoringNode &authoring_node : behavior.program)
            {
                root_child_ids.push_back(static_cast<NodeId>(result.bank.nodes.size()));
                max_concurrent_voices = std::max(max_concurrent_voices, LowerNode(context, authoring_node, root_node));
            }

            if (!root_child_ids.empty())
            {
                result.bank.nodes[root_node].first_child = static_cast<std::uint32_t>(result.bank.node_children.size());
                result.bank.node_children.insert(result.bank.node_children.end(), root_child_ids.begin(), root_child_ids.end());
            }
            result.bank.nodes[root_node].child_count = static_cast<std::uint32_t>(root_child_ids.size());
            result.bank.nodes[root_node].asset_count = 0;
            result.bank.nodes[root_node].loop_count = 0;

            if (result.bank.nodes[root_node].child_count == 0)
                result.diagnostics.push_back(MakeError(behavior.location, "behavior '" + behavior.id + "' compiled to an empty program"));

            const std::uint32_t first_parameter = static_cast<std::uint32_t>(result.bank.program_parameters.size());
            result.bank.program_parameters.insert(result.bank.program_parameters.end(),
                                                  context.program_parameters.begin(),
                                                  context.program_parameters.end());

            CompiledProgram compiled_program;
            compiled_program.id = program_id;
            compiled_program.root_node = root_node;
            compiled_program.first_node = first_node;
            compiled_program.node_count = static_cast<std::uint32_t>(result.bank.nodes.size() - first_node);
            compiled_program.first_parameter = first_parameter;
            compiled_program.parameter_count = static_cast<std::uint32_t>(context.program_parameters.size());
            compiled_program.parameter_slot_count = compiled_program.parameter_count;
            compiled_program.max_concurrent_voices = max_concurrent_voices;
            compiled_program.spatialization.mode = behavior.spatialization.mode;
            compiled_program.spatialization.min_distance = behavior.spatialization.min_distance;
            compiled_program.spatialization.max_distance = behavior.spatialization.max_distance;
            compiled_program.spatialization.attenuation = behavior.spatialization.attenuation;

            if (compiled_program.spatialization.mode == SpatializationMode::Pan)
            {
                if (compiled_program.spatialization.min_distance < 0.0f)
                    result.diagnostics.push_back(MakeError(behavior.spatialization.location, "behavior '" + behavior.id + "' spatialization minDistance must be >= 0"));

                if (compiled_program.spatialization.max_distance <= compiled_program.spatialization.min_distance)
                    result.diagnostics.push_back(MakeError(behavior.spatialization.location, "behavior '" + behavior.id + "' spatialization maxDistance must be > minDistance"));
            }

            result.bank.max_program_node_count = std::max(result.bank.max_program_node_count, compiled_program.node_count);
            result.bank.max_program_parameter_slot_count = std::max(result.bank.max_program_parameter_slot_count, compiled_program.parameter_slot_count);
            result.bank.max_program_concurrent_voices = std::max(result.bank.max_program_concurrent_voices, compiled_program.max_concurrent_voices);
            result.bank.programs.push_back(compiled_program);

            CompiledBehavior compiled_behavior;
            compiled_behavior.id = behavior_it->second;
            compiled_behavior.program_id = program_id;
            compiled_behavior.first_tag = first_tag;
            compiled_behavior.tag_count = static_cast<std::uint32_t>(result.bank.behavior_tags.size() - first_tag);
            compiled_behavior.first_condition = first_condition;
            compiled_behavior.condition_count = static_cast<std::uint32_t>(result.bank.conditions.size() - first_condition);
            result.bank.behaviors.push_back(compiled_behavior);
        }

        return result;
    }
} // namespace decl_audio::compiler
