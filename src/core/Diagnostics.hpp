#pragma once

#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace decl_audio
{
    enum class DiagnosticSeverity : unsigned char
    {
        Warning,
        Error
    };

    struct SourceLocation final
    {
        std::string file_path;
        std::string object_path;
    };

    struct Diagnostic final
    {
        DiagnosticSeverity severity = DiagnosticSeverity::Error;
        SourceLocation location;
        std::string message;
    };

    [[nodiscard]] inline bool HasErrors(std::span<const Diagnostic> diagnostics) noexcept
    {
        for (const Diagnostic &d : diagnostics)
        {
            if (d.severity == DiagnosticSeverity::Error)
                return true;
        }
        return false;
    }

    [[nodiscard]] inline SourceLocation MakeLocation(std::string_view file_path, std::string_view object_path = {})
    {
        SourceLocation loc;
        loc.file_path = std::string(file_path);
        loc.object_path = std::string(object_path);
        return loc;
    }

    [[nodiscard]] inline Diagnostic MakeError(std::string_view source_path, std::string message)
    {
        Diagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.location = MakeLocation(source_path);
        d.message = std::move(message);
        return d;
    }

    [[nodiscard]] inline Diagnostic MakeError(std::string_view source_path, std::string_view object_path, std::string message)
    {
        Diagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.location = MakeLocation(source_path, object_path);
        d.message = std::move(message);
        return d;
    }

    [[nodiscard]] inline Diagnostic MakeError(const SourceLocation &location, std::string message)
    {
        Diagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.location = location;
        d.message = std::move(message);
        return d;
    }

    [[nodiscard]] inline std::string FormatSourceLocation(const SourceLocation &location)
    {
        std::ostringstream stream;
        stream << location.file_path;
        if (!location.object_path.empty())
            stream << ": " << location.object_path;
        return stream.str();
    }

    [[nodiscard]] inline std::string DumpDiagnostics(std::span<const Diagnostic> diagnostics)
    {
        std::ostringstream stream;
        for (const Diagnostic &d : diagnostics)
        {
            stream << (d.severity == DiagnosticSeverity::Error ? "error" : "warning")
                   << ": "
                   << FormatSourceLocation(d.location)
                   << ": "
                   << d.message
                   << '\n';
        }
        return stream.str();
    }
} // namespace decl_audio
