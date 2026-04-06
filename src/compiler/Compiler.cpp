#include "pch.h"

#include "Compiler.hpp"

#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace decl_audio::compiler
{
    namespace
    {
        constexpr NodeId kInvalidNodeId = std::numeric_limits<NodeId>::max();
        constexpr std::uint16_t kInvalidParameterSlot = std::numeric_limits<std::uint16_t>::max();

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
            {
                return it->second;
            }

            const AssetId id = static_cast<AssetId>(bank.asset_paths.size());
            bank.asset_name_to_id.emplace(asset_name, id);
            bank.asset_paths.push_back(asset_name);
            return id;
        }

        [[nodiscard]] const char *ToString(ComparisonOp op)
        {
            switch (op)
            {
            case ComparisonOp::Less:
                return "<";
            case ComparisonOp::LessOrEqual:
                return "<=";
            case ComparisonOp::Equal:
                return "==";
            case ComparisonOp::GreaterOrEqual:
                return ">=";
            case ComparisonOp::Greater:
                return ">";
            }

            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(NodeType type)
        {
            switch (type)
            {
            case NodeType::Sequence:
                return "sequence";
            case NodeType::Select:
                return "select";
            case NodeType::Blend:
                return "blend";
            case NodeType::OneShot:
                return "oneshot";
            case NodeType::Loop:
                return "loop";
            case NodeType::Random:
                return "random";
            }

            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(SpatializationMode mode)
        {
            switch (mode)
            {
            case SpatializationMode::None:
                return "none";
            case SpatializationMode::Pan:
                return "pan";
            }

            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(AttenuationMode attenuation)
        {
            switch (attenuation)
            {
            case AttenuationMode::Linear:
                return "linear";
            }

            return "<invalid>";
        }

        struct ProgramLoweringContext final
        {
            CompiledBank &bank;
            std::string_view behavior_id;
            std::vector<Diagnostic> &diagnostics;
            const std::unordered_set<ParameterId> &declared_parameter_ids;
            std::unordered_map<ParameterId, std::uint16_t> parameter_slots;
            std::vector<ParameterId> program_parameters;
        };

        [[nodiscard]] std::uint16_t RequireProgramParameterSlot(ProgramLoweringContext &context,
                                                                const AuthoringNode &authoring_node) noexcept
        {
            if (authoring_node.parameter.empty())
            {
                context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' blend nodes require a parameter"));
                return kInvalidParameterSlot;
            }

            const auto parameter_it = context.bank.parameter_name_to_id.find(authoring_node.parameter);
            if (parameter_it == context.bank.parameter_name_to_id.end() ||
                !context.declared_parameter_ids.contains(parameter_it->second))
            {
                context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' blend node parameter '" + authoring_node.parameter + "' must be declared in behavior.parameters"));
                return kInvalidParameterSlot;
            }

            const ParameterId parameter_id = parameter_it->second;
            const auto slot_it = context.parameter_slots.find(parameter_id);
            if (slot_it != context.parameter_slots.end())
            {
                return slot_it->second;
            }

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
            {
                context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' " + std::string(leaf_name) + " nodes do not allow children"));
            }

            if (!authoring_node.parameter.empty())
            {
                context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' " + std::string(leaf_name) + " nodes do not allow parameters"));
            }
        }

        void ValidateStructuralShape(const AuthoringNode &authoring_node,
                                     ProgramLoweringContext &context,
                                     std::string_view node_name,
                                     const bool parameter_allowed)
        {
            if (!authoring_node.assets.empty())
            {
                context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' " + std::string(node_name) + " nodes do not allow assets"));
            }

            if (!parameter_allowed && !authoring_node.parameter.empty())
            {
                context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' " + std::string(node_name) + " nodes do not allow parameters"));
            }
        }

        [[nodiscard]] std::uint32_t LowerNode(ProgramLoweringContext &context,
                                              const AuthoringNode &authoring_node,
                                              const NodeId parent_id)
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

            switch (authoring_node.type)
            {
            case AuthoringNodeType::Sequence:
            {
                context.bank.nodes[node_id].type = NodeType::Sequence;
                ValidateStructuralShape(authoring_node, context, "sequence", false);
                if (authoring_node.children.empty())
                {
                    context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' has an empty sequence node"));
                }

                for (const AuthoringNode &child : authoring_node.children)
                {
                    const NodeId child_id = static_cast<NodeId>(context.bank.nodes.size());
                    const std::uint32_t child_concurrency = LowerNode(context, child, node_id);
                    child_ids.push_back(child_id);
                    max_concurrent_voices = std::max(max_concurrent_voices, child_concurrency);
                }
                break;
            }

            case AuthoringNodeType::Select:
            {
                context.bank.nodes[node_id].type = NodeType::Select;
                ValidateStructuralShape(authoring_node, context, "select", false);
                if (authoring_node.children.empty())
                {
                    context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' select nodes require at least one child"));
                }

                for (const AuthoringNode &child : authoring_node.children)
                {
                    const NodeId child_id = static_cast<NodeId>(context.bank.nodes.size());
                    const std::uint32_t child_concurrency = LowerNode(context, child, node_id);
                    child_ids.push_back(child_id);
                    max_concurrent_voices = std::max(max_concurrent_voices, child_concurrency);
                }
                break;
            }

            case AuthoringNodeType::Blend:
            {
                context.bank.nodes[node_id].type = NodeType::Blend;
                ValidateStructuralShape(authoring_node, context, "blend", true);
                context.bank.nodes[node_id].parameter_slot = RequireProgramParameterSlot(context, authoring_node);
                if (authoring_node.children.size() != 2)
                {
                    context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' blend nodes require exactly two children"));
                }

                for (const AuthoringNode &child : authoring_node.children)
                {
                    const NodeId child_id = static_cast<NodeId>(context.bank.nodes.size());
                    const std::uint32_t child_concurrency = LowerNode(context, child, node_id);
                    child_ids.push_back(child_id);
                    max_concurrent_voices += child_concurrency;
                }
                break;
            }

            case AuthoringNodeType::OneShot:
            {
                context.bank.nodes[node_id].type = NodeType::OneShot;
                ValidateLeafShape(authoring_node, context, "oneshot");
                if (authoring_node.assets.size() != 1)
                {
                    context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' oneshot nodes require exactly one asset"));
                }

                context.bank.nodes[node_id].first_asset = static_cast<std::uint32_t>(context.bank.node_assets.size());
                for (const std::string &asset_name : authoring_node.assets)
                {
                    context.bank.node_assets.push_back(InternAsset(context.bank, asset_name));
                }
                max_concurrent_voices = 1;
                break;
            }

            case AuthoringNodeType::Loop:
            {
                context.bank.nodes[node_id].type = NodeType::Loop;
                ValidateLeafShape(authoring_node, context, "loop");
                if (authoring_node.assets.size() != 1)
                {
                    context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' loop nodes require exactly one asset"));
                }
                if (authoring_node.loop_count == 0)
                {
                    context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' loop nodes do not allow loopCount = 0"));
                }

                context.bank.nodes[node_id].first_asset = static_cast<std::uint32_t>(context.bank.node_assets.size());
                for (const std::string &asset_name : authoring_node.assets)
                {
                    context.bank.node_assets.push_back(InternAsset(context.bank, asset_name));
                }
                max_concurrent_voices = 1;
                break;
            }

            case AuthoringNodeType::Random:
            {
                context.bank.nodes[node_id].type = NodeType::Random;
                ValidateLeafShape(authoring_node, context, "random");
                if (authoring_node.assets.empty())
                {
                    context.diagnostics.push_back(MakeError(authoring_node.location, "behavior '" + std::string(context.behavior_id) + "' random nodes require at least one asset"));
                }

                context.bank.nodes[node_id].first_asset = static_cast<std::uint32_t>(context.bank.node_assets.size());
                for (const std::string &asset_name : authoring_node.assets)
                {
                    context.bank.node_assets.push_back(InternAsset(context.bank, asset_name));
                }
                max_concurrent_voices = 1;
                break;
            }
            }

            if (!child_ids.empty())
            {
                context.bank.nodes[node_id].first_child = static_cast<std::uint32_t>(context.bank.node_children.size());
                context.bank.node_children.insert(context.bank.node_children.end(), child_ids.begin(), child_ids.end());
            }
            context.bank.nodes[node_id].child_count = static_cast<std::uint32_t>(child_ids.size());
            if (context.bank.nodes[node_id].type == NodeType::OneShot ||
                context.bank.nodes[node_id].type == NodeType::Loop ||
                context.bank.nodes[node_id].type == NodeType::Random)
            {
                context.bank.nodes[node_id].asset_count = static_cast<std::uint32_t>(context.bank.node_assets.size() - context.bank.nodes[node_id].first_asset);
            }
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
            {
                result.diagnostics.push_back(MakeError(behavior.location, "duplicate behavior id '" + behavior.id + "'"));
            }

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
            {
                result.bank.behavior_tags.push_back(InternName(result.bank.tag_name_to_id, tag_name));
            }

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
                const NodeId child_id = static_cast<NodeId>(result.bank.nodes.size());
                const std::uint32_t child_concurrency = LowerNode(context, authoring_node, root_node);
                root_child_ids.push_back(child_id);
                max_concurrent_voices = std::max(max_concurrent_voices, child_concurrency);
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
            {
                result.diagnostics.push_back(MakeError(behavior.location, "behavior '" + behavior.id + "' compiled to an empty program"));
            }

            const std::uint32_t first_parameter = static_cast<std::uint32_t>(result.bank.program_parameters.size());
            result.bank.program_parameters.insert(result.bank.program_parameters.end(), context.program_parameters.begin(), context.program_parameters.end());

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
                {
                    result.diagnostics.push_back(MakeError(behavior.spatialization.location, "behavior '" + behavior.id + "' spatialization minDistance must be >= 0"));
                }

                if (compiled_program.spatialization.max_distance <= compiled_program.spatialization.min_distance)
                {
                    result.diagnostics.push_back(MakeError(behavior.spatialization.location, "behavior '" + behavior.id + "' spatialization maxDistance must be > minDistance"));
                }
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

    std::string DumpCompiledBank(const CompiledBank &bank)
    {
        std::ostringstream stream;
        stream << "CompiledBank\n";
        stream << "  behaviors: " << bank.behaviors.size() << '\n';
        stream << "  programs: " << bank.programs.size() << '\n';
        stream << "  nodes: " << bank.nodes.size() << '\n';
        stream << "  assets: " << bank.asset_paths.size() << '\n';
        stream << "  tags: " << bank.tag_name_to_id.size() << '\n';
        stream << "  parameters: " << bank.parameter_name_to_id.size() << '\n';

        for (const CompiledBehavior &behavior : bank.behaviors)
        {
            stream << "Behavior[" << behavior.id << "]\n";
            stream << "  program: " << behavior.program_id << '\n';
            const CompiledProgram &program = bank.GetProgram(behavior.program_id);
            stream << "  spatialization: mode=" << ToString(program.spatialization.mode);
            if (program.spatialization.mode != SpatializationMode::None)
            {
                stream << " minDistance=" << program.spatialization.min_distance
                       << " maxDistance=" << program.spatialization.max_distance
                       << " attenuation=" << ToString(program.spatialization.attenuation);
            }
            stream << '\n';
            stream << "  tags:";

            for (TagId tag_id : bank.GetBehaviorTags(behavior.id))
            {
                stream << ' ' << tag_id;
            }

            stream << '\n';
            stream << "  conditions:\n";

            for (const CompiledCondition &condition : bank.GetBehaviorConditions(behavior.id))
            {
                stream << "    parameter=" << condition.parameter_id
                       << ' ' << ToString(condition.op)
                       << ' ' << condition.literal
                       << '\n';
            }

            stream << "  nodes:\n";

            for (const CompiledNode &node : bank.GetProgramNodes(behavior.program_id))
            {
                stream << "    type=" << ToString(node.type)
                       << " parent=" << node.parent
                       << " gain=" << node.authored_gain
                       << " children=" << node.child_count
                       << " paramSlot=" << node.parameter_slot
                       << " loopCount=" << node.loop_count
                       << " assets=";

                for (AssetId asset_id : bank.GetNodeAssets(node))
                {
                    stream << ' ' << asset_id << '[' << bank.GetAssetPath(asset_id) << ']';
                }

                stream << '\n';
            }
        }

        return stream.str();
    }
} // namespace decl_audio::compiler
