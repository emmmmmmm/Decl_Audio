#pragma once

#include <cstdint>
#include <variant>

#include "../compiler/CompilerTypes.hpp"
#include "../core/BankId.hpp"
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
        BankId bank_id{}; // resolves to a slot in the audio thread's bank table
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

    struct SetParameterCommand final
    {
        InstanceId instance_id = 0;
        compiler::ParameterId parameter_id = 0;
        float value = 0.0f;
    };

    struct RequestStopCommand final
    {
        InstanceId instance_id = 0;
    };

    struct SetListenerPositionCommand final
    {
        Vec3 position{};
    };

    struct SetMasterGainCommand final
    {
        float gain = 1.0f;
    };

    using AudioCommand = std::variant<
        CreateInstanceCommand,
        SetVolumeCommand,
        SetPositionCommand,
        SetParameterCommand,
        RequestStopCommand,
        SetListenerPositionCommand,
        SetMasterGainCommand>;
} // namespace decl_audio::playback
