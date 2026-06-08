#pragma once

#include <string>
#include <variant>

#include "../compiler/CompilerTypes.hpp"
#include "../core/vec3.hpp"

namespace decl_audio::runtime
{
    // Tag/parameter commands carry the raw *name*; the control thread interns it
    // against the VocabularyRegistry while draining (single-writer, section 4.4).
    // entity_id was already a string here - this extends the same idiom.
    struct SetTagCommand final
    {
        std::string entity_id;
        std::string tag_name;
    };
    struct SetTransientTagCommand final
    {
        std::string entity_id;
        std::string tag_name;
    };

    struct RemoveTagCommand final
    {
        std::string entity_id;
        std::string tag_name;
    };

    struct SetFloatValueCommand final
    {
        std::string entity_id;
        std::string parameter_name;
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

    struct SetListenerPositionCommand final
    {
        Vec3 position{};
    };

    struct DestroyEntityCommand final
    {
        std::string entity_id;
    };

    struct SetGlobalTagCommand final
    {
        std::string tag_name;
    };
    struct RemoveGlobalTagCommand final
    {
        std::string tag_name;
    };
    struct SetGlobalFloatValueCommand final
    {
        std::string parameter_name;
        float value = 0.0f;
    };

    struct SetMasterGainCommand final
    {
        float gain = 1.0f;
    };

    using HostCommand = std::variant<
        SetTagCommand,
        SetTransientTagCommand,
        RemoveTagCommand,
        SetFloatValueCommand,
        SetEntityVolumeCommand,
        SetEntityPositionCommand,
        SetListenerPositionCommand,
        DestroyEntityCommand,
        SetGlobalTagCommand,
        RemoveGlobalTagCommand,
        SetGlobalFloatValueCommand,
        SetMasterGainCommand>;
} // namespace decl_audio::runtime
