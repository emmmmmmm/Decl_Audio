#include <filesystem>
#include <iostream>
#include <string>

#include "../include/Decl_Audio/Decl_Audio.h"
#include "../src/compiler/CompiledBank.hpp"
#include "../src/compiler/Compiler.hpp"
#include "../src/core/Engine.hpp"

namespace
{
bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }

    return true;
}

std::filesystem::path GetFixturePath(const char *file_name)
{
    return std::filesystem::path(__FILE__).parent_path() / "data" / file_name;
}

bool TestValidFixtureCompilesAndLoads()
{
    const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(!compile_result.HasErrors(), "valid fixture should compile without errors"))
    {
        std::cerr << decl_audio::compiler::DumpDiagnostics(compile_result.diagnostics);
        return false;
    }

    if (!Expect(compile_result.bank.behaviors.size() == 1, "valid fixture should compile one behavior"))
        return false;
    if (!Expect(compile_result.bank.programs.size() == 1, "valid fixture should compile one program"))
        return false;
    if (!Expect(compile_result.bank.containers.size() == 3, "sequence container should flatten into three compiled containers"))
        return false;
    if (!Expect(compile_result.bank.asset_paths.size() == 4, "valid fixture should discover four referenced assets"))
        return false;
    if (!Expect(compile_result.bank.GetBehaviorTags(0).size() == 2, "valid fixture should intern two match tags"))
        return false;
    if (!Expect(compile_result.bank.GetBehaviorConditions(0).size() == 1, "valid fixture should compile one condition"))
        return false;

    const std::span<const decl_audio::compiler::CompiledContainer> containers = compile_result.bank.GetProgramContainers(0);
    if (!Expect(containers[0].type == decl_audio::compiler::ContainerType::OneShot, "first compiled container should be oneshot"))
        return false;
    if (!Expect(containers[1].type == decl_audio::compiler::ContainerType::Random, "second compiled container should be random"))
        return false;
    if (!Expect(containers[2].type == decl_audio::compiler::ContainerType::Loop, "third compiled container should be loop"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "Engine::LoadBehaviors should accept the valid fixture"))
        return false;

    DeclAudioEngine *api_engine = nullptr;
    if (!Expect(CreateEngine(&config, &api_engine), "CreateEngine should succeed with a valid config"))
        return false;
    if (!Expect(api_engine != nullptr, "CreateEngine should initialize the engine handle"))
        return false;
    if (!Expect(LoadBehaviors(api_engine, fixture_path.string().c_str()), "C API LoadBehaviors should accept the valid fixture"))
    {
        DestroyEngine(api_engine);
        return false;
    }

    DestroyEngine(api_engine);

    std::cout << decl_audio::compiler::DumpCompiledBank(compile_result.bank);
    return true;
}

bool TestInvalidFixtureFailsLoudly()
{
    const std::filesystem::path fixture_path = GetFixturePath("InvalidBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(compile_result.HasErrors(), "invalid fixture should produce compiler errors"))
        return false;
    if (!Expect(!compile_result.diagnostics.empty(), "invalid fixture should emit diagnostics"))
        return false;

    const std::string diagnostics = decl_audio::compiler::DumpDiagnostics(compile_result.diagnostics);
    if (!Expect(diagnostics.find("duplicate behavior id 'movement.invalid'") != std::string::npos, "diagnostics should report duplicate ids"))
        return false;
    if (!Expect(diagnostics.find("oneshot containers require exactly one asset") != std::string::npos, "diagnostics should report invalid oneshot asset counts"))
        return false;
    if (!Expect(diagnostics.find("random containers require at least one asset") != std::string::npos, "diagnostics should report empty random asset lists"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(!engine.LoadBehaviors(fixture_path.string().c_str()), "Engine::LoadBehaviors should reject the invalid fixture"))
        return false;

    DeclAudioEngine *api_engine = nullptr;
    if (!Expect(CreateEngine(&config, &api_engine), "CreateEngine should still succeed for the invalid fixture test"))
        return false;
    if (!Expect(!LoadBehaviors(api_engine, fixture_path.string().c_str()), "C API LoadBehaviors should reject the invalid fixture"))
    {
        DestroyEngine(api_engine);
        return false;
    }

    DestroyEngine(api_engine);

    std::cout << diagnostics;
    return true;
}
} // namespace

bool RunCompilerTests()
{
    if (!TestValidFixtureCompilesAndLoads())
        return false;

    if (!TestInvalidFixtureFailsLoudly())
        return false;

    std::cout << "Compiler tests passed\n";
    return true;
}
