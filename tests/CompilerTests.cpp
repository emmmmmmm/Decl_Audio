#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

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

    EngineConfig GetTestConfig()
    {
        EngineConfig config = GetDefaultConfig();
        config.backend = DECL_AUDIO_BACKEND_SILENT;
        return config;
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

        if (!Expect(compile_result.bank.behaviors.size() == 2, "valid fixture should compile two behaviors"))
            return false;
        if (!Expect(compile_result.bank.programs.size() == 2, "valid fixture should compile two programs"))
            return false;
        if (!Expect(compile_result.bank.containers.size() == 4, "valid fixture should flatten into four compiled containers"))
            return false;
        if (!Expect(compile_result.bank.asset_paths.size() == 2, "valid fixture should discover two unique referenced assets"))
            return false;
        if (!Expect(compile_result.bank.GetBehaviorTags(0).size() == 2, "valid fixture should intern two match tags"))
            return false;
        if (!Expect(compile_result.bank.GetBehaviorConditions(0).size() == 1, "valid fixture should compile one condition"))
            return false;
        if (!Expect(compile_result.bank.GetBehaviorTags(1).size() == 1, "resolver fixture should intern one match tag"))
            return false;
        if (!Expect(compile_result.bank.GetBehaviorConditions(1).size() == 1, "resolver fixture should compile one condition"))
            return false;

        const std::span<const decl_audio::compiler::CompiledContainer> containers = compile_result.bank.GetProgramContainers(0);
        if (!Expect(containers[0].type == decl_audio::compiler::ContainerType::OneShot, "first compiled container should be oneshot"))
            return false;
        if (!Expect(containers[1].type == decl_audio::compiler::ContainerType::Random, "second compiled container should be random"))
            return false;
        if (!Expect(containers[2].type == decl_audio::compiler::ContainerType::Loop, "third compiled container should be loop"))
            return false;

        const std::span<const decl_audio::compiler::CompiledContainer> resolver_containers = compile_result.bank.GetProgramContainers(1);
        if (!Expect(resolver_containers.size() == 1, "resolver program should compile one loop container"))
            return false;
        if (!Expect(resolver_containers[0].type == decl_audio::compiler::ContainerType::Loop, "resolver program should compile to a loop"))
            return false;

        auto config = GetTestConfig();

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

        auto config = GetTestConfig();

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

    bool TestReservedRuntimeParamsNeedNoDeclarations()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ParameterForwardingBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

        if (!Expect(!compile_result.HasErrors(), "phase 7 fixture should compile without errors"))
        {
            std::cerr << decl_audio::compiler::DumpDiagnostics(compile_result.diagnostics);
            return false;
        }

        if (!Expect(compile_result.bank.parameter_name_to_id.empty(), "phase 7 fixture should not need any declared match parameters"))
            return false;

        return true;
    }

    bool TestSpatializationFixtureCompilesProgramSettings()
    {
        const std::filesystem::path fixture_path = GetFixturePath("SpatializationBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

        if (!Expect(!compile_result.HasErrors(), "phase 7.5 spatialization fixture should compile without errors"))
        {
            std::cerr << decl_audio::compiler::DumpDiagnostics(compile_result.diagnostics);
            return false;
        }

        const decl_audio::compiler::CompiledProgram &mono_program = compile_result.bank.GetProgram(compile_result.bank.GetProgramId("spatial.mono"));
        if (!Expect(mono_program.spatialization.mode == decl_audio::compiler::SpatializationMode::Pan, "spatialized mono program should compile with pan mode"))
            return false;
        if (!Expect(mono_program.spatialization.min_distance == 1.0f, "spatialized mono program should retain minDistance"))
            return false;
        if (!Expect(mono_program.spatialization.max_distance == 5.0f, "spatialized mono program should retain maxDistance"))
            return false;
        if (!Expect(mono_program.spatialization.attenuation == decl_audio::compiler::AttenuationMode::Linear, "spatialized mono program should compile linear attenuation"))
            return false;

        const decl_audio::compiler::CompiledProgram &stereo_program = compile_result.bank.GetProgram(compile_result.bank.GetProgramId("spatial.stereo"));
        if (!Expect(stereo_program.spatialization.mode == decl_audio::compiler::SpatializationMode::Pan, "spatialized stereo program should compile with pan mode"))
            return false;

        return true;
    }

    bool TestSpatializationValidationFailsLoudly()
    {
        constexpr std::string_view kInvalidSpatializationSource = R"json(
{
  "behaviors": [
    {
      "id": "spatial.invalid.parse",
      "spatialization": {
        "minDistance": 1.0,
        "attenuation": "quadratic",
        "mode": "pan"
      },
      "program": [
        {
          "type": "oneshot",
          "asset": "audio/test_48_24_1ch.wav"
        }
      ]
    }
  ]
}
)json";

        const decl_audio::compiler::ParseResult parse_result = decl_audio::compiler::ParseAuthoringJson(kInvalidSpatializationSource, "SpatializationValidation.parse.json");
        if (!Expect(parse_result.HasErrors(), "invalid spatialization parse fixture should emit parse errors"))
            return false;

        const std::string parse_diagnostics = decl_audio::compiler::DumpDiagnostics(parse_result.diagnostics);
        if (!Expect(parse_diagnostics.find(".mode: is not a supported spatialization field") != std::string::npos, "spatialization diagnostics should report unsupported fields"))
            return false;
        if (!Expect(parse_diagnostics.find(".maxDistance: is required") != std::string::npos, "spatialization diagnostics should report missing maxDistance"))
            return false;
        if (!Expect(parse_diagnostics.find(".attenuation: has unsupported attenuation mode") != std::string::npos, "spatialization diagnostics should report unsupported attenuation"))
            return false;

        constexpr std::string_view kInvalidSpatializationRangeSource = R"json(
{
  "behaviors": [
    {
      "id": "spatial.invalid.compile",
      "spatialization": {
        "minDistance": 4.0,
        "maxDistance": 2.0,
        "attenuation": "linear"
      },
      "program": [
        {
          "type": "oneshot",
          "asset": "audio/test_48_24_1ch.wav"
        }
      ]
    }
  ]
}
)json";

        const decl_audio::compiler::ParseResult compile_parse_result = decl_audio::compiler::ParseAuthoringJson(kInvalidSpatializationRangeSource, "SpatializationValidation.compile.json");
        if (!Expect(!compile_parse_result.HasErrors(), "range validation fixture should parse before compile validation"))
            return false;

        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::CompileAuthoringDocument(compile_parse_result.document);
        if (!Expect(compile_result.HasErrors(), "invalid spatialization range fixture should emit compile errors"))
            return false;

        const std::string compile_diagnostics = decl_audio::compiler::DumpDiagnostics(compile_result.diagnostics);
        if (!Expect(compile_diagnostics.find("spatialization maxDistance must be > minDistance") != std::string::npos, "spatialization diagnostics should report invalid ranges"))
            return false;

        return true;
    }

    bool TestAudioConfigDefaultsAndValidation()
    {
        auto audio_config = GetDefaultConfig();

        if (!Expect(audio_config.sample_rate == 48000u, "default audio config should use the engine sample rate"))
            return false;
        if (!Expect(audio_config.output_channel_count == 2u, "default audio config should default to stereo output"))
            return false;
        if (!Expect(audio_config.callback_frame_count == 1024u, "default audio config should expose a callback block size"))
            return false;
        if (!Expect(audio_config.max_instances == 256u, "default audio config should expose the runtime voice capacity"))
            return false;
        if (!Expect(audio_config.max_block_frames == 4096u, "default audio config should reserve a bounded render block slack"))
            return false;
        if (!Expect(audio_config.backend == DECL_AUDIO_BACKEND_PLATFORM_DEFAULT, "default audio config should use the platform backend"))
            return false;

        audio_config.output_channel_count = 3;
        DeclAudioEngine *engine = nullptr;
        if (!Expect(!CreateEngine(&audio_config, &engine), "CreateEngine should reject unsupported audio channel counts"))
            return false;
        if (!Expect(engine == nullptr, "CreateEngine should leave the output engine pointer null on invalid audio config"))
            return false;

        audio_config = GetDefaultConfig();
        audio_config.max_instances = 0;
        if (!Expect(!CreateEngine(&audio_config, &engine), "CreateEngine should reject zero runtime voice capacity"))
            return false;
        if (!Expect(engine == nullptr, "CreateEngine should leave the output engine pointer null on invalid runtime capacity"))
            return false;

        audio_config = GetDefaultConfig();
        audio_config.max_block_frames = audio_config.callback_frame_count - 1;
        if (!Expect(!CreateEngine(&audio_config, &engine), "CreateEngine should reject runtime block capacities smaller than the callback size"))
            return false;
        if (!Expect(engine == nullptr, "CreateEngine should leave the output engine pointer null when block capacity is undersized"))
            return false;

        return true;
    }
} // namespace

bool RunCompilerTests()
{
    if (!TestValidFixtureCompilesAndLoads())
        return false;

    if (!TestInvalidFixtureFailsLoudly())
        return false;

    if (!TestAudioConfigDefaultsAndValidation())
        return false;

    if (!TestReservedRuntimeParamsNeedNoDeclarations())
        return false;

    if (!TestSpatializationFixtureCompilesProgramSettings())
        return false;

    if (!TestSpatializationValidationFailsLoudly())
        return false;

    std::cout << "Compiler tests passed\n";
    return true;
}
