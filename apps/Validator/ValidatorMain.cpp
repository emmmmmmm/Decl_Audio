#include <iostream>
#include <string_view>
#include <vector>

#include "../../src/assets/AssetBank.hpp"
#include "../../src/compiler/Compiler.hpp"
#include "../../src/core/BankSerializer.hpp"

static int RunValidateJson(const char *json_path)
{
    const decl_audio::compiler::CompileResult result =
        decl_audio::compiler::LoadCompiledBankFromJsonFile(json_path);

    if (result.HasErrors())
    {
        std::cerr << decl_audio::DumpDiagnostics(result.diagnostics);
        return 1;
    }

    const decl_audio::assets::LoadResult asset_result =
        decl_audio::assets::LoadAssetBank(result.bank, json_path);

    if (asset_result.HasErrors())
    {
        std::cerr << decl_audio::DumpDiagnostics(asset_result.diagnostics);
        return 1;
    }

    std::cout << decl_audio::compiler::DumpCompiledBank(result.bank);
    std::cout << decl_audio::assets::DumpAssetBank(result.bank, asset_result.bank);
    return 0;
}

static int RunBuild(const char *json_path, const char *output_path)
{
    const decl_audio::compiler::CompileResult compile_result =
        decl_audio::compiler::LoadCompiledBankFromJsonFile(json_path);

    if (compile_result.HasErrors())
    {
        std::cerr << decl_audio::DumpDiagnostics(compile_result.diagnostics);
        return 1;
    }

    const decl_audio::assets::LoadResult asset_result =
        decl_audio::assets::LoadAssetBank(compile_result.bank, json_path);

    if (asset_result.HasErrors())
    {
        std::cerr << decl_audio::DumpDiagnostics(asset_result.diagnostics);
        return 1;
    }

    std::vector<decl_audio::Diagnostic> write_diags;
    const bool ok = decl_audio::serialization::WriteBankToFile(
        output_path,
        compile_result.bank,
        asset_result.bank,
        write_diags);

    if (!ok)
    {
        std::cerr << decl_audio::DumpDiagnostics(write_diags);
        return 1;
    }

    std::cout << "Bank written to: " << output_path << '\n';
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage:\n"
                  << "  Decl_Audio.Validator validate <behavior-bank.json>\n"
                  << "  Decl_Audio.Validator build    <behavior-bank.json> <output.dacbank>\n";
        return 1;
    }

    const std::string_view subcommand = argv[1];

    if (subcommand == "validate")
    {
        if (argc != 3)
        {
            std::cerr << "Usage: Decl_Audio.Validator validate <behavior-bank.json>\n";
            return 1;
        }
        return RunValidateJson(argv[2]);
    }

    if (subcommand == "build")
    {
        if (argc != 4)
        {
            std::cerr << "Usage: Decl_Audio.Validator build <behavior-bank.json> <output.dacbank>\n";
            return 1;
        }
        return RunBuild(argv[2], argv[3]);
    }

    std::cerr << "Unknown subcommand '" << subcommand << "'\n"
              << "Usage:\n"
              << "  Decl_Audio.Validator validate <behavior-bank.json>\n"
              << "  Decl_Audio.Validator build    <behavior-bank.json> <output.dacbank>\n";
    return 1;
}
