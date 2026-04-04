#include "pch.h"

#include "Diagnostics.hpp"

#include <sstream>

namespace decl_audio::compiler
{
    std::string FormatSourceLocation(const SourceLocation &location)
    {
        std::ostringstream stream;
        stream << location.file_path;

        if (!location.object_path.empty())
            stream << ": " << location.object_path;

        return stream.str();
    }

    std::string DumpDiagnostics(std::span<const Diagnostic> diagnostics)
    {
        std::ostringstream stream;

        for (const Diagnostic &diagnostic : diagnostics)
        {
            stream << (diagnostic.severity == DiagnosticSeverity::Error ? "error" : "warning")
                   << ": "
                   << FormatSourceLocation(diagnostic.location)
                   << ": "
                   << diagnostic.message
                   << '\n';
        }

        return stream.str();
    }
} // namespace decl_audio::compiler
