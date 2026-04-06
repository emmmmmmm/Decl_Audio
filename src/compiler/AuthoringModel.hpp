#pragma once

#include "CompilerTypes.hpp"

#include <string>
#include <vector>

namespace decl_audio::compiler
{
    struct AuthoringCondition final
    {
        SourceLocation location;
        std::string parameter;
        ComparisonOp op = ComparisonOp::Equal;
        float literal = 0.0f;
    };

    struct AuthoringNode final
    {
        SourceLocation location;
        AuthoringNodeType type = AuthoringNodeType::OneShot;
        std::vector<std::string> assets;
        std::vector<AuthoringNode> children;
        std::string parameter;
        float volume = 1.0f;
        std::int32_t loop_count = 0;
    };

    struct AuthoringSpatializationSettings final
    {
        SourceLocation location;
        SpatializationMode mode = SpatializationMode::None;
        float min_distance = 0.0f;
        float max_distance = 0.0f;
        AttenuationMode attenuation = AttenuationMode::Linear;
    };

    struct AuthoringBehavior final
    {
        SourceLocation location;
        std::string id;
        std::vector<std::string> match_tags;
        std::vector<AuthoringCondition> match_conditions;
        std::vector<AuthoringNode> program;
        std::vector<std::string> parameters;
        AuthoringSpatializationSettings spatialization;
    };

    struct AuthoringDocument final
    {
        std::vector<AuthoringBehavior> behaviors;
    };
} // namespace decl_audio::compiler
