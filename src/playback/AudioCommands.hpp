#pragma once

#include <cstdint>
#include <variant>

#include "../compiler/CompilerTypes.hpp"
#include "../core/vec3.hpp"

namespace decl_audio::playback
{
    using InstanceId = std::uint64_t;

    struct CreateInstanceCommand final
    {
        InstanceId instance_id = 0;
        compiler::ProgramId program_id = 0;
        Vec3 position{};
        float volume = 1.0f;
    };

    struct SetVolumeCommand final
    {
        InstanceId instance_id = 0;
        float volume = 1.0f;
    };

    struct SetPositionCommand final
    {
        InstanceId instance_id = 0;
        Vec3 position{};
    };

    struct RequestStopCommand final
    {
        InstanceId instance_id = 0;
    };

    using AudioCommand = std::variant<
        CreateInstanceCommand,
        SetVolumeCommand,
        SetPositionCommand,
        RequestStopCommand>;
} // namespace decl_audio::playback
