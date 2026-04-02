#include <iostream>

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
        std::cerr << decl_audio::compiler::DumpDiagnostics(result.diagnostics);
        return 1;
    }

    std::cout << decl_audio::compiler::DumpCompiledBank(result.bank);
    return 0;
}
