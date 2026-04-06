#pragma once

#include <span>
#include "CompilerTypes.hpp"

#include <span>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace decl_audio::compiler
{
    struct CompiledCondition final
    {
        ParameterId parameter_id = 0;
        ComparisonOp op = ComparisonOp::Equal;
        float literal = 0.0f;
    };

    struct CompiledBehavior final
    {
        BehaviorId id = 0;
        ProgramId program_id = 0;
        std::uint32_t first_tag = 0;
        std::uint32_t tag_count = 0;
        std::uint32_t first_condition = 0;
        std::uint32_t condition_count = 0;
    };

    struct CompiledNode final
    {
        NodeType type = NodeType::OneShot;
        NodeId parent = std::numeric_limits<NodeId>::max();
        float authored_gain = 1.0f;
        std::uint32_t first_child = 0;
        std::uint32_t child_count = 0;
        std::uint16_t parameter_slot = std::numeric_limits<std::uint16_t>::max();
        std::uint32_t first_asset = 0;
        std::uint32_t asset_count = 0;
        std::int32_t loop_count = 0;
    };

    using CompiledContainer = CompiledNode;

    struct CompiledSpatializationSettings final
    {
        SpatializationMode mode = SpatializationMode::None;
        float min_distance = 0.0f;
        float max_distance = 0.0f;
        AttenuationMode attenuation = AttenuationMode::Linear;
    };

    struct CompiledProgram final
    {
        ProgramId id = 0;
        NodeId root_node = 0;
        std::uint32_t first_node = 0;
        std::uint32_t node_count = 0;
        std::uint32_t first_parameter = 0;
        std::uint32_t parameter_count = 0;
        std::uint32_t parameter_slot_count = 0;
        std::uint32_t max_concurrent_voices = 0;
        CompiledSpatializationSettings spatialization;
    };

    struct CompiledBank final
    {
        std::vector<CompiledBehavior> behaviors;
        std::vector<CompiledProgram> programs;
        std::vector<CompiledNode> nodes;

        std::vector<TagId> behavior_tags;
        std::vector<CompiledCondition> conditions;
        std::vector<NodeId> node_children;
        std::vector<AssetId> node_assets;
        std::vector<ParameterId> program_parameters;

        std::vector<std::string> asset_paths;
        std::uint32_t max_program_node_count = 0;
        std::uint32_t max_program_parameter_slot_count = 0;
        std::uint32_t max_program_concurrent_voices = 0;

        std::unordered_map<std::string, BehaviorId> behavior_name_to_id;
        std::unordered_map<std::string, ProgramId> program_name_to_id;
        std::unordered_map<std::string, TagId> tag_name_to_id;
        std::unordered_map<std::string, ParameterId> parameter_name_to_id;
        std::unordered_map<std::string, AssetId> asset_name_to_id;

        [[nodiscard]] const CompiledBehavior &GetBehavior(BehaviorId id) const
        {
            return behaviors.at(static_cast<std::size_t>(id));
        }

        [[nodiscard]] const CompiledProgram &GetProgram(ProgramId id) const
        {
            return programs.at(static_cast<std::size_t>(id));
        }

        [[nodiscard]] std::span<const TagId> GetBehaviorTags(BehaviorId id) const
        {
            const CompiledBehavior &behavior = GetBehavior(id);
            return std::span<const TagId>(behavior_tags).subspan(behavior.first_tag, behavior.tag_count);
        }

        [[nodiscard]] std::span<const CompiledCondition> GetBehaviorConditions(BehaviorId id) const
        {
            const CompiledBehavior &behavior = GetBehavior(id);
            return std::span<const CompiledCondition>(conditions).subspan(behavior.first_condition, behavior.condition_count);
        }

        [[nodiscard]] std::span<const CompiledNode> GetProgramNodes(ProgramId id) const
        {
            const CompiledProgram &program = GetProgram(id);
            return std::span<const CompiledNode>(nodes).subspan(program.first_node, program.node_count);
        }

        [[nodiscard]] std::span<const CompiledContainer> GetProgramContainers(ProgramId id) const
        {
            return GetProgramNodes(id);
        }

        [[nodiscard]] std::span<const NodeId> GetNodeChildren(const CompiledNode &node) const
        {
            return std::span<const NodeId>(node_children).subspan(node.first_child, node.child_count);
        }

        [[nodiscard]] std::span<const AssetId> GetNodeAssets(const CompiledNode &node) const
        {
            return std::span<const AssetId>(node_assets).subspan(node.first_asset, node.asset_count);
        }

        [[nodiscard]] std::span<const AssetId> GetContainerAssets(const CompiledContainer &container) const
        {
            return GetNodeAssets(container);
        }

        [[nodiscard]] std::span<const ParameterId> GetProgramParameters(ProgramId id) const
        {
            const CompiledProgram &program = GetProgram(id);
            return std::span<const ParameterId>(program_parameters).subspan(program.first_parameter, program.parameter_count);
        }

        [[nodiscard]] const std::string &GetAssetPath(AssetId id) const
        {
            return asset_paths.at(static_cast<std::size_t>(id));
        }

        [[nodiscard]] BehaviorId GetBehaviorId(std::string_view name) const
        {
            return behavior_name_to_id.at(std::string(name));
        }

        [[nodiscard]] ProgramId GetProgramId(std::string_view name) const
        {
            return program_name_to_id.at(std::string(name));
        }

        [[nodiscard]] TagId GetTagId(std::string_view name) const
        {
            return tag_name_to_id.at(std::string(name));
        }

        [[nodiscard]] ParameterId GetParameterId(std::string_view name) const
        {
            return parameter_name_to_id.at(std::string(name));
        }

        [[nodiscard]] AssetId GetAssetId(std::string_view name) const
        {
            return asset_name_to_id.at(std::string(name));
        }
    };
} // namespace decl_audio::compiler
