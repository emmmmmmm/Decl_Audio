#pragma once

#include "AuthoringModel.hpp"
#include "CompiledBank.hpp"
#include "../core/Diagnostics.hpp"

#include <filesystem>
#include <string_view>
#include <vector>

namespace decl_audio::compiler
{
    struct ParseResult final
    {
        AuthoringDocument document;
        std::vector<decl_audio::Diagnostic> diagnostics;

        [[nodiscard]] bool HasErrors() const noexcept
        {
            return decl_audio::HasErrors(diagnostics);
        }
    };

    struct CompileResult final
    {
        CompiledBank bank;
        std::vector<decl_audio::Diagnostic> diagnostics;

        [[nodiscard]] bool HasErrors() const noexcept
        {
            return decl_audio::HasErrors(diagnostics);
        }
    };

    [[nodiscard]] ParseResult ParseAuthoringJson(std::string_view source_text, std::string_view source_path);
    [[nodiscard]] CompileResult CompileAuthoringDocument(const AuthoringDocument &document);
    [[nodiscard]] CompileResult LoadCompiledBankFromJsonFile(const std::filesystem::path &source_path);
    [[nodiscard]] std::string DumpCompiledBank(const CompiledBank &bank);
} // namespace decl_audio::compiler
