#pragma once

#include <string>
#include <variant>

#include "../compiler/CompilerTypes.hpp"

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

    struct DestroyEntityCommand final
    {
        std::string entity_id;
    };

    using HostCommand = std::variant<
        SetTagCommand,
        RemoveTagCommand,
        SetFloatValueCommand,
        DestroyEntityCommand>;
} // namespace decl_audio::runtime
