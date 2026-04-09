#pragma once

#include "../core/Diagnostics.hpp"

#include <cstdint>
#include <limits>

namespace decl_audio::compiler
{
    using BehaviorId = std::uint32_t;
    using ProgramId = std::uint32_t;
    using NodeId = std::uint32_t;
    using TagId = std::uint32_t;
    using ParameterId = std::uint32_t;
    using AssetId = std::uint32_t;

    constexpr NodeId kInvalidNodeId = std::numeric_limits<NodeId>::max();
    constexpr std::uint16_t kInvalidParameterSlot = std::numeric_limits<std::uint16_t>::max();

    enum class ComparisonOp : std::uint8_t
    {
        Less,
        LessOrEqual,
        Equal,
        GreaterOrEqual,
        Greater
    };

    enum class SpatializationMode : std::uint8_t
    {
        None,
        Pan
    };

    enum class AttenuationMode : std::uint8_t
    {
        Linear
    };

    enum class NodeType : std::uint8_t
    {
        Sequence,
        Select,
        Blend,
        OneShot,
        Loop,
        Random
    };

    enum class StopMode : std::uint8_t
    {
        Immediate, // fade out over stop_fade_frames, skip any remaining sequence children
        Graceful,  // finish current loop pass, advance sequence normally
    };

    using ContainerType = NodeType;
} // namespace decl_audio::compiler
