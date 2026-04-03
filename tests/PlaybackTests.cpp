#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

#include "../include/Decl_Audio/Decl_Audio.h"
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

bool ExpectNear(float actual, float expected, float tolerance, const char *message)
{
    if (std::fabs(actual - expected) > tolerance)
    {
        std::cerr << "FAILED: " << message << " expected " << expected << " got " << actual << '\n';
        return false;
    }

    return true;
}

std::filesystem::path GetFixturePath(const char *file_name)
{
    return std::filesystem::path(__FILE__).parent_path() / "data" / file_name;
}

bool TestCreateInstanceProducesExpectedStereoSamples()
{
    const std::filesystem::path fixture_path = GetFixturePath("PlaybackBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(!compile_result.HasErrors(), "phase 4 playback fixture should compile"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 4 playback fixture should load"))
        return false;

    const decl_audio::compiler::ProgramId program_id = compile_result.bank.GetProgramId("playback.oneshot");
    const decl_audio::compiler::AssetId asset_id = compile_result.bank.GetAssetId("audio/test_48_24_1ch.wav");
    const decl_audio::assets::DecodedBuffer &buffer = engine.GetAssetBank().GetBuffer(asset_id);
    if (!Expect(buffer.frame_count > 32, "mono playback fixture should contain enough frames for the render test"))
        return false;

    constexpr decl_audio::playback::InstanceId kInstanceId = 1001;
    engine.SubmitCreateInstanceForTesting(kInstanceId, program_id, 0.25f, Vec3{});

    constexpr std::uint32_t kFramesToRender = 16;
    std::vector<float> output(static_cast<std::size_t>(kFramesToRender) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(output.data(), kFramesToRender);

    if (!Expect(engine.GetActiveAudioInstanceCountForTesting() == 1, "oneshot instance should still be active after a short render"))
        return false;

    for (std::uint32_t frame_index = 0; frame_index < kFramesToRender; ++frame_index)
    {
        const float expected = buffer.samples[frame_index] * 0.5f * 0.25f;
        const std::size_t sample_index = static_cast<std::size_t>(frame_index) * decl_audio::playback::AudioRuntime::OutputChannelCount;
        if (!ExpectNear(output[sample_index + 0], expected, 1e-6f, "left channel should match duplicated mono source"))
            return false;
        if (!ExpectNear(output[sample_index + 1], expected, 1e-6f, "right channel should match duplicated mono source"))
            return false;
    }

    return true;
}

bool TestSetVolumeAndPositionCommandsApplyOnNextBlock()
{
    const std::filesystem::path fixture_path = GetFixturePath("PlaybackBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(!compile_result.HasErrors(), "phase 4 set-command fixture should compile"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 4 set-command fixture should load"))
        return false;

    const decl_audio::compiler::ProgramId program_id = compile_result.bank.GetProgramId("playback.loop");
    const decl_audio::compiler::AssetId asset_id = compile_result.bank.GetAssetId("audio/test_48_24_1ch.wav");
    const decl_audio::assets::DecodedBuffer &buffer = engine.GetAssetBank().GetBuffer(asset_id);
    if (!Expect(buffer.frame_count > 4, "loop playback fixture should contain enough frames for the set-command test"))
        return false;

    constexpr decl_audio::playback::InstanceId kInstanceId = 2002;
    engine.SubmitCreateInstanceForTesting(kInstanceId, program_id, 1.0f, Vec3{});

    std::vector<float> warmup_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(warmup_output.data(), 1);

    engine.SubmitSetVolumeForTesting(kInstanceId, 0.5f);
    engine.SubmitSetPositionForTesting(kInstanceId, Vec3{1.0f, 2.0f, 3.0f});

    std::vector<float> output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(output.data(), 1);

    decl_audio::playback::InstanceSnapshot snapshot;
    if (!Expect(engine.TryGetAudioInstanceSnapshotForTesting(kInstanceId, snapshot), "loop instance should still be active after volume/position update"))
        return false;

    if (!Expect(snapshot.position == Vec3{1.0f, 2.0f, 3.0f}, "SetPosition should update the stored instance position"))
        return false;
    if (!ExpectNear(snapshot.volume, 0.5f, 1e-6f, "SetVolume should update the stored instance volume"))
        return false;

    const float expected = buffer.samples[1] * 0.75f * 0.5f;
    if (!ExpectNear(output[0], expected, 1e-6f, "updated volume should affect the next rendered block"))
        return false;
    if (!ExpectNear(output[1], expected, 1e-6f, "updated volume should affect both stereo channels"))
        return false;

    return true;
}

bool TestRequestStopRetiresLoopAfterCurrentPass()
{
    const std::filesystem::path fixture_path = GetFixturePath("PlaybackBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(!compile_result.HasErrors(), "phase 4 stop fixture should compile"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 4 stop fixture should load"))
        return false;

    const decl_audio::compiler::ProgramId program_id = compile_result.bank.GetProgramId("playback.loop");
    const decl_audio::compiler::AssetId asset_id = compile_result.bank.GetAssetId("audio/test_48_24_1ch.wav");
    const decl_audio::assets::DecodedBuffer &buffer = engine.GetAssetBank().GetBuffer(asset_id);
    if (!Expect(buffer.frame_count > 16, "loop playback fixture should contain enough frames for the stop test"))
        return false;

    constexpr decl_audio::playback::InstanceId kInstanceId = 3003;
    constexpr std::uint32_t kPrefixFrames = 9;

    engine.SubmitCreateInstanceForTesting(kInstanceId, program_id, 1.0f, Vec3{});

    std::vector<float> prefix_output(static_cast<std::size_t>(kPrefixFrames) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(prefix_output.data(), kPrefixFrames);

    engine.SubmitRequestStopForTesting(kInstanceId);

    const std::uint32_t remaining_frames = static_cast<std::uint32_t>(buffer.frame_count - kPrefixFrames);
    std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(stop_output.data(), remaining_frames + 8);

    if (!Expect(engine.GetActiveAudioInstanceCountForTesting() == 0, "RequestStop should retire the loop after its current pass"))
        return false;

    for (std::uint32_t frame_index = 0; frame_index < remaining_frames; ++frame_index)
    {
        const float expected = buffer.samples[kPrefixFrames + frame_index] * 0.75f;
        const std::size_t sample_index = static_cast<std::size_t>(frame_index) * decl_audio::playback::AudioRuntime::OutputChannelCount;
        if (!ExpectNear(stop_output[sample_index + 0], expected, 1e-6f, "stopped loop should play through the current pass on the left channel"))
            return false;
        if (!ExpectNear(stop_output[sample_index + 1], expected, 1e-6f, "stopped loop should play through the current pass on the right channel"))
            return false;
    }

    for (std::size_t sample_index = static_cast<std::size_t>(remaining_frames) * decl_audio::playback::AudioRuntime::OutputChannelCount;
         sample_index < stop_output.size();
         ++sample_index)
    {
        if (!ExpectNear(stop_output[sample_index], 0.0f, 1e-6f, "retired loop should leave trailing samples silent"))
            return false;
    }

    return true;
}

bool TestStubBackendPumpsPlaybackWithoutOutputBuffer()
{
    const std::filesystem::path fixture_path = GetFixturePath("PlaybackBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(!compile_result.HasErrors(), "phase 4 stub backend fixture should compile"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 4 stub backend fixture should load"))
        return false;

    const decl_audio::compiler::ProgramId program_id = compile_result.bank.GetProgramId("playback.loop");
    constexpr decl_audio::playback::InstanceId kInstanceId = 4004;
    engine.SubmitCreateInstanceForTesting(kInstanceId, program_id, 1.0f, Vec3{});
    engine.PumpAudioForTesting(8);

    if (!Expect(engine.GetActiveAudioInstanceCountForTesting() == 1, "stub backend should drive the audio runtime without exposing output"))
        return false;

    return true;
}

bool TestResolverCreatesAndStopsLoopOnTagMatch()
{
    const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(!compile_result.HasErrors(), "phase 5 tag-match fixture should compile"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 5 tag-match fixture should load"))
        return false;

    const decl_audio::compiler::AssetId asset_id = compile_result.bank.GetAssetId("audio/test_48_24_1ch.wav");
    const decl_audio::assets::DecodedBuffer &buffer = engine.GetAssetBank().GetBuffer(asset_id);
    if (!Expect(buffer.frame_count > 8, "phase 5 loop fixture should contain enough frames"))
        return false;

    engine.SetTag("player", "resolver.active");
    engine.SetValue("player", "speed", 1.0f);
    engine.Update();

    std::vector<float> startup_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(startup_output.data(), 1);

    if (!Expect(engine.GetActiveAudioInstanceCountForTesting() == 1, "new match should create one audio instance"))
        return false;

    const float expected_start = buffer.samples[0] * 0.4f;
    if (!ExpectNear(startup_output[0], expected_start, 1e-6f, "resolver-created loop should render expected left-channel samples"))
        return false;
    if (!ExpectNear(startup_output[1], expected_start, 1e-6f, "resolver-created loop should render expected right-channel samples"))
        return false;

    engine.RemoveTag("player", "resolver.active");
    engine.Update();

    const std::uint32_t remaining_frames = static_cast<std::uint32_t>(buffer.frame_count - 1);
    std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(stop_output.data(), remaining_frames + 8);

    if (!Expect(engine.GetActiveAudioInstanceCountForTesting() == 0, "lost match should retire the bound instance after the current pass"))
        return false;

    for (std::uint32_t frame_index = 0; frame_index < remaining_frames; ++frame_index)
    {
        const float expected = buffer.samples[frame_index + 1] * 0.4f;
        const std::size_t sample_index = static_cast<std::size_t>(frame_index) * decl_audio::playback::AudioRuntime::OutputChannelCount;
        if (!ExpectNear(stop_output[sample_index + 0], expected, 1e-6f, "stopped resolver loop should finish the current pass on the left channel"))
            return false;
        if (!ExpectNear(stop_output[sample_index + 1], expected, 1e-6f, "stopped resolver loop should finish the current pass on the right channel"))
            return false;
    }

    for (std::size_t sample_index = static_cast<std::size_t>(remaining_frames) * decl_audio::playback::AudioRuntime::OutputChannelCount;
         sample_index < stop_output.size();
         ++sample_index)
    {
        if (!ExpectNear(stop_output[sample_index], 0.0f, 1e-6f, "retired resolver loop should leave trailing samples silent"))
            return false;
    }

    return true;
}

bool TestDestroyEntityStopsBoundLoop()
{
    const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(!compile_result.HasErrors(), "phase 5 destroy fixture should compile"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 5 destroy fixture should load"))
        return false;

    const decl_audio::compiler::AssetId asset_id = compile_result.bank.GetAssetId("audio/test_48_24_1ch.wav");
    const decl_audio::assets::DecodedBuffer &buffer = engine.GetAssetBank().GetBuffer(asset_id);
    if (!Expect(buffer.frame_count > 8, "phase 5 destroy loop fixture should contain enough frames"))
        return false;

    engine.SetTag("player", "resolver.active");
    engine.SetValue("player", "speed", 1.0f);
    engine.Update();

    std::vector<float> startup_output(static_cast<std::size_t>(4) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(startup_output.data(), 4);

    if (!Expect(engine.GetActiveAudioInstanceCountForTesting() == 1, "destroy test should start one bound loop"))
        return false;

    engine.DestroyEntity("player");
    engine.Update();

    const std::uint32_t remaining_frames = static_cast<std::uint32_t>(buffer.frame_count - 4);
    std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(stop_output.data(), remaining_frames + 8);

    if (!Expect(engine.GetActiveAudioInstanceCountForTesting() == 0, "DestroyEntity should stop every bound instance after its current pass"))
        return false;

    for (std::size_t sample_index = static_cast<std::size_t>(remaining_frames) * decl_audio::playback::AudioRuntime::OutputChannelCount;
         sample_index < stop_output.size();
         ++sample_index)
    {
        if (!ExpectNear(stop_output[sample_index], 0.0f, 1e-6f, "destroyed entity should not leave a looping instance alive"))
            return false;
    }

    return true;
}

bool TestResolverForwardsBoundParams()
{
    const std::filesystem::path fixture_path = GetFixturePath("ParameterForwardingBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    if (!Expect(!compile_result.HasErrors(), "phase 7 forwarding fixture should compile"))
        return false;

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

    decl_audio::Engine engine(config);
    if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 7 forwarding fixture should load"))
        return false;

    const decl_audio::compiler::AssetId asset_id = compile_result.bank.GetAssetId("audio/test_48_24_1ch.wav");
    const decl_audio::assets::DecodedBuffer &buffer = engine.GetAssetBank().GetBuffer(asset_id);
    if (!Expect(buffer.frame_count > 4, "phase 7 fixture should contain enough frames"))
        return false;

    engine.SetValue("player", "volume", 0.5f);
    engine.SetPosition("player", 1.0f, 2.0f, 3.0f);
    engine.SetTag("player", "resolver.active");
    engine.Update();

    std::vector<float> startup_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(startup_output.data(), 1);

    if (!Expect(engine.GetActiveAudioInstanceCountForTesting() == 1, "phase 7 match should create one bound instance"))
        return false;

    decl_audio::playback::InstanceSnapshot snapshot;
    constexpr decl_audio::playback::InstanceId kResolverInstanceId = 1;
    if (!Expect(engine.TryGetAudioInstanceSnapshotForTesting(kResolverInstanceId, snapshot), "phase 7 bound instance should expose a testing snapshot"))
        return false;
    if (!ExpectNear(snapshot.volume, 0.5f, 1e-6f, "initial bound volume should come from the current entity value"))
        return false;
    if (!Expect(snapshot.position == Vec3{1.0f, 2.0f, 3.0f}, "initial bound position should come from the current entity value"))
        return false;

    const float expected_start = buffer.samples[0] * 0.4f * 0.5f;
    if (!ExpectNear(startup_output[0], expected_start, 1e-6f, "initial forwarded volume should affect the left channel"))
        return false;
    if (!ExpectNear(startup_output[1], expected_start, 1e-6f, "initial forwarded volume should affect the right channel"))
        return false;

    engine.SetValue("player", "volume", 0.25f);
    engine.SetPosition("player", 4.0f, 5.0f, 6.0f);
    engine.Update();

    std::vector<float> updated_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
    engine.RenderAudioForTesting(updated_output.data(), 1);

    if (!Expect(engine.TryGetAudioInstanceSnapshotForTesting(kResolverInstanceId, snapshot), "phase 7 bound instance should remain active after parameter updates"))
        return false;
    if (!ExpectNear(snapshot.volume, 0.25f, 1e-6f, "updated bound volume should stick on the audio instance"))
        return false;
    if (!Expect(snapshot.position == Vec3{4.0f, 5.0f, 6.0f}, "updated bound position should stick on the audio instance"))
        return false;

    const float expected_updated = buffer.samples[1] * 0.4f * 0.25f;
    if (!ExpectNear(updated_output[0], expected_updated, 1e-6f, "updated forwarded volume should affect the next block on the left channel"))
        return false;
    if (!ExpectNear(updated_output[1], expected_updated, 1e-6f, "updated forwarded volume should affect the next block on the right channel"))
        return false;

    return true;
}
} // namespace

bool RunPlaybackTests()
{
    if (!TestCreateInstanceProducesExpectedStereoSamples())
        return false;

    if (!TestSetVolumeAndPositionCommandsApplyOnNextBlock())
        return false;

    if (!TestRequestStopRetiresLoopAfterCurrentPass())
        return false;

    if (!TestStubBackendPumpsPlaybackWithoutOutputBuffer())
        return false;

    if (!TestResolverCreatesAndStopsLoopOnTagMatch())
        return false;

    if (!TestDestroyEntityStopsBoundLoop())
        return false;

    if (!TestResolverForwardsBoundParams())
        return false;

    std::cout << "Playback tests passed\n";
    return true;
}
