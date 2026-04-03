#pragma once

#include <span>
#include "CompilerTypes.hpp"

#include <span>
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

    struct CompiledContainer final
    {
        ContainerType type = ContainerType::OneShot;
        float volume = 1.0f;
        std::uint32_t first_asset = 0;
        std::uint32_t asset_count = 0;
        std::int32_t loop_count = 0;
    };

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
        std::uint32_t first_container = 0;
        std::uint32_t container_count = 0;
        CompiledSpatializationSettings spatialization;
    };

    struct CompiledBank final
    {
        std::vector<CompiledBehavior> behaviors;
        std::vector<CompiledProgram> programs;
        std::vector<CompiledContainer> containers;

        std::vector<TagId> behavior_tags;
        std::vector<CompiledCondition> conditions;
        std::vector<AssetId> container_assets;

        std::vector<std::string> asset_paths;

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

        [[nodiscard]] std::span<const CompiledContainer> GetProgramContainers(ProgramId id) const
        {
            const CompiledProgram &program = GetProgram(id);
            return std::span<const CompiledContainer>(containers).subspan(program.first_container, program.container_count);
        }

        [[nodiscard]] std::span<const AssetId> GetContainerAssets(const CompiledContainer &container) const
        {
            return std::span<const AssetId>(container_assets).subspan(container.first_asset, container.asset_count);
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
