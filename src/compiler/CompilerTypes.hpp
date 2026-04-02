#pragma once

#include <cstdint>
#include <string>

namespace decl_audio::compiler
{
    using BehaviorId = std::uint32_t;
    using ProgramId = std::uint32_t;
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

    enum class ContainerType : std::uint8_t
    {
        OneShot,
        Loop,
        Random
    };

    enum class AuthoringContainerType : std::uint8_t
    {
        OneShot,
        Loop,
        Random,
        Sequence
    };

    struct SourceLocation final
    {
        std::string file_path;
        std::uint32_t line = 1;
        std::uint32_t column = 1;
    };
} // namespace decl_audio::compiler
