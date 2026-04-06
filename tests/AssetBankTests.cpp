#include <filesystem>
#include <iostream>
#include <string>

#include "../src/assets/AssetBank.hpp"
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

    bool TestValidAssetsDecodeAndLoad()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

        if (!Expect(!compile_result.HasErrors(), "valid asset fixture should compile"))
            return false;

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "valid asset fixture should load and decode"))
            return false;

        const decl_audio::assets::AssetBank &asset_bank = engine.GetAssetBank();
        if (!Expect(asset_bank.buffers.size() == compile_result.bank.asset_paths.size(), "asset bank should decode every referenced asset"))
            return false;

        const decl_audio::compiler::AssetId mono_asset_id = compile_result.bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &mono_buffer = asset_bank.GetBuffer(mono_asset_id);
        if (!Expect(mono_buffer.channel_count == 1, "mono fixture should decode as mono"))
            return false;
        if (!Expect(mono_buffer.sample_rate == decl_audio::assets::kRequiredSampleRate, "mono fixture should decode at 48 kHz"))
            return false;
        if (!Expect(mono_buffer.frame_count > 0, "mono fixture should contain frames"))
            return false;
        if (!Expect(mono_buffer.samples.size() == mono_buffer.frame_count * mono_buffer.channel_count, "mono sample storage should match frame count"))
            return false;

        const decl_audio::compiler::AssetId stereo_asset_id = compile_result.bank.GetAssetId("audio/test_48_24_2ch.wav");
        const decl_audio::assets::DecodedBuffer &stereo_buffer = asset_bank.GetBuffer(stereo_asset_id);
        if (!Expect(stereo_buffer.channel_count == 2, "stereo fixture should decode as stereo"))
            return false;
        if (!Expect(stereo_buffer.sample_rate == decl_audio::assets::kRequiredSampleRate, "stereo fixture should decode at 48 kHz"))
            return false;
        if (!Expect(stereo_buffer.frame_count > 0, "stereo fixture should contain frames"))
            return false;
        if (!Expect(stereo_buffer.samples.size() == stereo_buffer.frame_count * stereo_buffer.channel_count, "stereo sample storage should match frame count"))
            return false;

        std::cout << decl_audio::assets::DumpAssetBank(compile_result.bank, asset_bank);
        return true;
    }

    bool TestSampleRateMismatchFailsLoudly()
    {
        const std::filesystem::path fixture_path = GetFixturePath("InvalidSampleRateBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

        if (!Expect(!compile_result.HasErrors(), "sample-rate mismatch fixture should compile"))
            return false;

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(!engine.LoadBehaviors(fixture_path.string().c_str()), "44.1 kHz asset should fail load"))
            return false;

        const std::string diagnostics = decl_audio::DumpDiagnostics(engine.GetLoadDiagnostics());
        if (!Expect(diagnostics.find("unsupported sample rate 44100 Hz; expected 48000 Hz") != std::string::npos, "load diagnostics should report the sample-rate mismatch"))
            return false;

        std::cout << diagnostics;
        return true;
    }
} // namespace

bool RunAssetBankTests()
{
    if (!TestValidAssetsDecodeAndLoad())
        return false;

    if (!TestSampleRateMismatchFailsLoudly())
        return false;

    std::cout << "AssetBank tests passed\n";
    return true;
}
