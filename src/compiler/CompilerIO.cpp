#include "pch.h"

#include "Compiler.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace decl_audio::compiler
{
    CompileResult LoadCompiledBankFromJsonFile(const std::filesystem::path &source_path)
    {
        CompileResult result;

        std::ifstream input(source_path, std::ios::binary);
        if (!input.is_open())
        {
            result.diagnostics.push_back(MakeError(source_path.string(), "failed to open authoring file"));
            return result;
        }

        const std::string source_text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        ParseResult parse_result = ParseAuthoringJson(source_text, source_path.string());

        result.diagnostics = parse_result.diagnostics;
        if (parse_result.HasErrors())
            return result;

        CompileResult compile_result = CompileAuthoringDocument(parse_result.document);
        result.bank = std::move(compile_result.bank);
        result.diagnostics.insert(result.diagnostics.end(), compile_result.diagnostics.begin(), compile_result.diagnostics.end());
        return result;
    }
} // namespace decl_audio::compiler
