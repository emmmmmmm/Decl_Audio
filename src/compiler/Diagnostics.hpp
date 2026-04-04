#pragma once

#include "CompilerTypes.hpp"

#include <span>
#include <string>
#include <string_view>

namespace decl_audio::compiler
{
    enum class DiagnosticSeverity : std::uint8_t
    {
        Warning,
        Error
    };

    struct Diagnostic final
    {
        DiagnosticSeverity severity = DiagnosticSeverity::Error;
        SourceLocation location;
        std::string message;
    };

    [[nodiscard]] inline SourceLocation MakeLocation(std::string_view source_path, std::string_view object_path = {})
    {
        SourceLocation location;
        location.file_path = std::string(source_path);
        location.object_path = std::string(object_path);
        return location;
    }

    [[nodiscard]] inline Diagnostic MakeError(std::string_view source_path, std::string message)
    {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostic.location = MakeLocation(source_path);
        diagnostic.message = std::move(message);
        return diagnostic;
    }

    [[nodiscard]] inline Diagnostic MakeError(std::string_view source_path, std::string_view object_path, std::string message)
    {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostic.location = MakeLocation(source_path, object_path);
        diagnostic.message = std::move(message);
        return diagnostic;
    }

    [[nodiscard]] inline Diagnostic MakeError(const SourceLocation &location, std::string message)
    {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostic.location = location;
        diagnostic.message = std::move(message);
        return diagnostic;
    }

    [[nodiscard]] std::string FormatSourceLocation(const SourceLocation &location);
    [[nodiscard]] std::string DumpDiagnostics(std::span<const Diagnostic> diagnostics);
} // namespace decl_audio::compiler
