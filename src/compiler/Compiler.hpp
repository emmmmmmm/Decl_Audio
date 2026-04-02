#pragma once

#include "AuthoringModel.hpp"
#include "CompiledBank.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

    struct ParseResult final
    {
        AuthoringDocument document;
        std::vector<Diagnostic> diagnostics;

        [[nodiscard]] bool HasErrors() const noexcept
        {
            for (const Diagnostic &diagnostic : diagnostics)
            {
                if (diagnostic.severity == DiagnosticSeverity::Error)
                    return true;
            }

            return false;
        }
    };

    struct CompileResult final
    {
        CompiledBank bank;
        std::vector<Diagnostic> diagnostics;

        [[nodiscard]] bool HasErrors() const noexcept
        {
            for (const Diagnostic &diagnostic : diagnostics)
            {
                if (diagnostic.severity == DiagnosticSeverity::Error)
                    return true;
            }

            return false;
        }
    };

    [[nodiscard]] ParseResult ParseAuthoringJson(std::string_view source_text, std::string_view source_path);
    [[nodiscard]] CompileResult CompileAuthoringDocument(const AuthoringDocument &document);
    [[nodiscard]] CompileResult LoadCompiledBankFromJsonFile(const std::filesystem::path &source_path);
    [[nodiscard]] std::string FormatSourceLocation(const SourceLocation &location);
    [[nodiscard]] std::string DumpDiagnostics(std::span<const Diagnostic> diagnostics);
    [[nodiscard]] std::string DumpCompiledBank(const CompiledBank &bank);
} // namespace decl_audio::compiler
