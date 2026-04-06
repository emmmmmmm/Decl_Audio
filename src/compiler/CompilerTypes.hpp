#pragma once

#include <cstdint>
#include <string>

namespace decl_audio::compiler
{
    using BehaviorId = std::uint32_t;
    using ProgramId = std::uint32_t;
    using NodeId = std::uint32_t;
    using TagId = std::uint32_t;
    using ParameterId = std::uint32_t;
    using AssetId = std::uint32_t;

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

    enum class AuthoringNodeType : std::uint8_t
    {
        Sequence,
        Select,
        Blend,
        OneShot,
        Loop,
        Random
    };

    using ContainerType = NodeType;
    using AuthoringContainerType = AuthoringNodeType;

    struct SourceLocation final
    {
        std::string file_path;
        std::string object_path;
    };
} // namespace decl_audio::compiler
