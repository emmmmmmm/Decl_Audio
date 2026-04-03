#pragma once

#include <string>
#include <variant>

#include "../compiler/CompilerTypes.hpp"
#include "../core/vec3.hpp"

namespace decl_audio::runtime
{
    struct SetTagCommand final
    {
        std::string entity_id;
        compiler::TagId tag_id = 0;
    };

    struct RemoveTagCommand final
    {
        std::string entity_id;
        compiler::TagId tag_id = 0;
    };

    struct SetFloatValueCommand final
    {
        std::string entity_id;
        compiler::ParameterId parameter_id = 0;
        float value = 0.0f;
    };

    struct SetEntityVolumeCommand final
    {
        std::string entity_id;
        float volume = 1.0f;
    };

    struct SetEntityPositionCommand final
    {
        std::string entity_id;
        Vec3 position{};
    };

    struct DestroyEntityCommand final
    {
        std::string entity_id;
    };

    using HostCommand = std::variant<
        SetTagCommand,
        RemoveTagCommand,
        SetFloatValueCommand,
        SetEntityVolumeCommand,
        SetEntityPositionCommand,
        DestroyEntityCommand>;
} // namespace decl_audio::runtime
