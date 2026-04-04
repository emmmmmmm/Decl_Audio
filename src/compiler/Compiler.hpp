#pragma once

#include "AuthoringModel.hpp"
#include "CompiledBank.hpp"
#include "Diagnostics.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace decl_audio::compiler
{
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
    [[nodiscard]] std::string DumpCompiledBank(const CompiledBank &bank);
} // namespace decl_audio::compiler
