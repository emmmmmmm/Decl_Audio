#include <iostream>

#include "../../src/assets/AssetBank.hpp"
#include "../../src/compiler/Compiler.hpp"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: Decl_Audio.Validator <behavior-bank.json>\n";
        return 1;
    }

    const decl_audio::compiler::CompileResult result = decl_audio::compiler::LoadCompiledBankFromJsonFile(argv[1]);

    if (result.HasErrors())
    {
        std::cerr << decl_audio::DumpDiagnostics(result.diagnostics);
        return 1;
    }

    const decl_audio::assets::LoadResult asset_result = decl_audio::assets::LoadAssetBank(result.bank, argv[1]);
    if (asset_result.HasErrors())
    {
        std::cerr << decl_audio::DumpDiagnostics(asset_result.diagnostics);
        return 1;
    }

    std::cout << decl_audio::compiler::DumpCompiledBank(result.bank);
    std::cout << decl_audio::assets::DumpAssetBank(result.bank, asset_result.bank);
    return 0;
}
