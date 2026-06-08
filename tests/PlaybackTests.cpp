#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../src/assets/AssetBank.hpp"
#include "../src/backends/StubBackend.hpp"
#include "../src/compiler/Compiler.hpp"
#include "../src/playback/AudioRuntime.hpp"
#include "../src/runtime/BehaviorResolver.hpp"
#include "../src/runtime/ControlRuntime.hpp"

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <crtdbg.h>
#include <process.h>
#include <cstdlib>
#include <windows.h>
#endif

const char *GetTestExecutablePath();

namespace
{
    constexpr int kDeathTestUnexpectedSuccessExitCode = 99;
    static constexpr std::uint32_t OutputChannelCount = 2;

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

    std::filesystem::path MakeDeathTestStartedFlagPath()
    {
        const auto unique_suffix = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
        return std::filesystem::temp_directory_path() / ("decl_audio_capacity_death_test_" + std::to_string(unique_suffix) + ".flag");
    }

    bool ProcessExitedWithCode(const int status, const int expected_exit_code)
    {
#if defined(__unix__) || defined(__APPLE__)
        return WIFEXITED(status) && WEXITSTATUS(status) == expected_exit_code;
#else
        return status == expected_exit_code;
#endif
    }

#if defined(_WIN32)
    void SuppressWindowsAbortDialogsForDeathTest() noexcept
    {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    }
#endif

    struct StereoMixGains final
    {
        float left = 1.0f;
        float right = 1.0f;
    };

    struct PlaybackTestRig final
    {
        decl_audio::compiler::CompiledBank compiled_bank;
        decl_audio::assets::AssetBank asset_bank;
        decl_audio::runtime::VocabularyRegistry vocabulary;
        decl_audio::runtime::ControlRuntime control_runtime{vocabulary};
        decl_audio::runtime::BehaviorResolver behavior_resolver;
        decl_audio::playback::AudioRuntime audio_runtime;
        decl_audio::backends::StubBackend stub_backend;

        bool LoadFixture(const std::filesystem::path &fixture_path,
                         const char *compile_message,
                         const char *load_message)
        {
            const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);
            if (!Expect(!compile_result.HasErrors(), compile_message))
                return false;

            const decl_audio::assets::LoadResult asset_result = decl_audio::assets::LoadAssetBank(compile_result.bank, fixture_path);
            if (!Expect(!asset_result.HasErrors(), load_message))
                return false;

            compiled_bank = compile_result.bank;
            asset_bank = asset_result.bank;
            behavior_resolver.Reset();
            vocabulary.MergeBank(compiled_bank); // intern + remap vocabulary to global ids
            audio_runtime.InstallBank(decl_audio::BankId{0u, 0u}, &compiled_bank, &asset_bank);
            return true;
        }

        void SubmitAudioCommand(const decl_audio::playback::AudioCommand &command)
        {
            audio_runtime.Submit(command);
        }

        void SetTag(const char *entity_id, const char *tag)
        {
            control_runtime.Submit(decl_audio::runtime::SetTagCommand{
                std::string(entity_id),
                std::string(tag)});
        }

        void RemoveTag(const char *entity_id, const char *tag)
        {
            control_runtime.Submit(decl_audio::runtime::RemoveTagCommand{
                std::string(entity_id),
                std::string(tag)});
        }

        void SetValue(const char *entity_id, const char *parameter, const float value)
        {
            if (std::string_view(parameter) == "volume")
            {
                control_runtime.Submit(decl_audio::runtime::SetEntityVolumeCommand{
                    std::string(entity_id),
                    value});
                return;
            }

            control_runtime.Submit(decl_audio::runtime::SetFloatValueCommand{
                std::string(entity_id),
                std::string(parameter),
                value});
        }

        void SetPosition(const char *entity_id, const float x, const float y, const float z)
        {
            control_runtime.Submit(decl_audio::runtime::SetEntityPositionCommand{
                std::string(entity_id),
                Vec3{x, y, z}});
        }

        void SetListenerPosition(const float x, const float y, const float z)
        {
            control_runtime.Submit(decl_audio::runtime::SetListenerPositionCommand{
                Vec3{x, y, z}});
        }

        void DestroyEntity(const char *entity_id)
        {
            control_runtime.Submit(decl_audio::runtime::DestroyEntityCommand{
                std::string(entity_id)});
        }

        void Update() noexcept
        {
            control_runtime.Tick();

            Vec3 listener_position;
            if (control_runtime.ListenerPositionChanged(listener_position))
            {
                audio_runtime.Submit(decl_audio::playback::SetListenerPositionCommand{
                    listener_position});
            }

            const decl_audio::runtime::ResolverBankView view{decl_audio::BankId{0u, 0u}, &compiled_bank, false};
            behavior_resolver.Resolve(
                control_runtime.GetWorldState(),
                std::span<const decl_audio::runtime::ResolverBankView>(&view, 1),
                [this](const decl_audio::playback::AudioCommand &command)
                {
                    audio_runtime.Submit(command);
                });

            control_runtime.ClearTransientTags();
        }

        void Render(float *output, const std::uint32_t frames) noexcept
        {
            audio_runtime.Render(output, frames);
        }

        void Pump(const std::uint32_t frames) noexcept
        {
            stub_backend.Pump(audio_runtime, frames);
        }
    };

    StereoMixGains ComputeExpectedSpatialMix(const Vec3 &source_position,
                                             const Vec3 &listener_position,
                                             const float min_distance,
                                             const float max_distance)
    {
        constexpr float kQuarterTurn = 0.78539816339744830962f;

        const Vec3 relative = Vec3::subtract(source_position, listener_position);
        const float distance = relative.magnitude();

        float attenuation = 1.0f;
        if (distance <= min_distance)
        {
            attenuation = 1.0f;
        }
        else if (distance >= max_distance)
        {
            attenuation = 0.0f;
        }
        else
        {
            attenuation = 1.0f - ((distance - min_distance) / (max_distance - min_distance));
        }

        float pan = 0.0f;
        if (distance > 0.0f)
        {
            pan = std::clamp(relative.x / distance, -1.0f, 1.0f);
        }

        const float angle = (pan + 1.0f) * kQuarterTurn;
        return StereoMixGains{
            std::cos(angle) * attenuation,
            std::sin(angle) * attenuation};
    }

    bool TestCreateInstanceProducesExpectedStereoSamples()
    {
        const std::filesystem::path fixture_path = GetFixturePath("PlaybackBehaviorBank.json");
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 4 playback fixture should compile", "phase 4 playback fixture should load"))
            return false;

        const decl_audio::compiler::ProgramId program_id = rig.compiled_bank.GetProgramId("playback.oneshot");
        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 32, "mono playback fixture should contain enough frames for the render test"))
            return false;

        constexpr decl_audio::playback::InstanceId kInstanceId = 1001;
        rig.SubmitAudioCommand(decl_audio::playback::CreateInstanceCommand{
            kInstanceId,
            program_id,
            Vec3{},
            0.25f});

        constexpr std::uint32_t kFramesToRender = 16;
        std::vector<float> output(static_cast<std::size_t>(kFramesToRender) * OutputChannelCount);
        rig.Render(output.data(), kFramesToRender);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 1, "oneshot instance should still be active after a short render"))
            return false;

        for (std::uint32_t frame_index = 0; frame_index < kFramesToRender; ++frame_index)
        {
            const float expected = buffer.samples[frame_index] * 0.5f * 0.25f;
            const std::size_t sample_index = static_cast<std::size_t>(frame_index) * OutputChannelCount;
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
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 4 set-command fixture should compile", "phase 4 set-command fixture should load"))
            return false;

        const decl_audio::compiler::ProgramId program_id = rig.compiled_bank.GetProgramId("playback.loop");
        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 4, "loop playback fixture should contain enough frames for the set-command test"))
            return false;

        constexpr decl_audio::playback::InstanceId kInstanceId = 2002;
        rig.SubmitAudioCommand(decl_audio::playback::CreateInstanceCommand{
            kInstanceId,
            program_id,
            Vec3{},
            1.0f});

        std::vector<float> warmup_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(warmup_output.data(), 1);

        rig.SubmitAudioCommand(decl_audio::playback::SetVolumeCommand{
            kInstanceId,
            0.5f});
        rig.SubmitAudioCommand(decl_audio::playback::SetPositionCommand{
            kInstanceId,
            Vec3{1.0f, 2.0f, 3.0f}});

        std::vector<float> output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(output.data(), 1);

        decl_audio::playback::InstanceSnapshot snapshot;
        if (!Expect(rig.audio_runtime.TryGetInstanceSnapshot(kInstanceId, snapshot), "loop instance should still be active after volume/position update"))
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
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 4 stop fixture should compile", "phase 4 stop fixture should load"))
            return false;

        const decl_audio::compiler::ProgramId program_id = rig.compiled_bank.GetProgramId("playback.loop");
        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 16, "loop playback fixture should contain enough frames for the stop test"))
            return false;

        constexpr decl_audio::playback::InstanceId kInstanceId = 3003;
        constexpr std::uint32_t kPrefixFrames = 9;

        rig.SubmitAudioCommand(decl_audio::playback::CreateInstanceCommand{
            kInstanceId,
            program_id,
            Vec3{},
            1.0f});

        std::vector<float> prefix_output(static_cast<std::size_t>(kPrefixFrames) * OutputChannelCount);
        rig.Render(prefix_output.data(), kPrefixFrames);

        rig.SubmitAudioCommand(decl_audio::playback::RequestStopCommand{kInstanceId});

        const std::uint32_t remaining_frames = static_cast<std::uint32_t>(buffer.frame_count - kPrefixFrames);
        std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * OutputChannelCount);
        rig.Render(stop_output.data(), remaining_frames + 8);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 0, "RequestStop should retire the loop after its current pass"))
            return false;

        for (std::uint32_t frame_index = 0; frame_index < remaining_frames; ++frame_index)
        {
            const float expected = buffer.samples[kPrefixFrames + frame_index] * 0.75f;
            const std::size_t sample_index = static_cast<std::size_t>(frame_index) * OutputChannelCount;
            if (!ExpectNear(stop_output[sample_index + 0], expected, 1e-6f, "stopped loop should play through the current pass on the left channel"))
                return false;
            if (!ExpectNear(stop_output[sample_index + 1], expected, 1e-6f, "stopped loop should play through the current pass on the right channel"))
                return false;
        }

        for (std::size_t sample_index = static_cast<std::size_t>(remaining_frames) * OutputChannelCount;
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
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 4 stub backend fixture should compile", "phase 4 stub backend fixture should load"))
            return false;

        const decl_audio::compiler::ProgramId program_id = rig.compiled_bank.GetProgramId("playback.loop");
        constexpr decl_audio::playback::InstanceId kInstanceId = 4004;
        rig.SubmitAudioCommand(decl_audio::playback::CreateInstanceCommand{
            kInstanceId,
            program_id,
            Vec3{},
            1.0f});
        rig.Pump(8);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 1, "stub backend should drive the audio runtime without exposing output"))
            return false;

        return true;
    }

    bool TestResolverCreatesAndStopsLoopOnTagMatch()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 5 tag-match fixture should compile", "phase 5 tag-match fixture should load"))
            return false;

        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 8, "phase 5 loop fixture should contain enough frames"))
            return false;

        rig.SetTag("player", "resolver.active");
        rig.SetValue("player", "speed", 1.0f);
        rig.Update();

        std::vector<float> startup_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(startup_output.data(), 1);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 1, "new match should create one audio instance"))
            return false;

        const float expected_start = buffer.samples[0] * 0.4f;
        if (!ExpectNear(startup_output[0], expected_start, 1e-6f, "resolver-created loop should render expected left-channel samples"))
            return false;
        if (!ExpectNear(startup_output[1], expected_start, 1e-6f, "resolver-created loop should render expected right-channel samples"))
            return false;

        rig.RemoveTag("player", "resolver.active");
        rig.Update();

        const std::uint32_t remaining_frames = static_cast<std::uint32_t>(buffer.frame_count - 1);
        std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * OutputChannelCount);
        rig.Render(stop_output.data(), remaining_frames + 8);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 0, "lost match should retire the bound instance after the current pass"))
            return false;

        for (std::uint32_t frame_index = 0; frame_index < remaining_frames; ++frame_index)
        {
            const float expected = buffer.samples[frame_index + 1] * 0.4f;
            const std::size_t sample_index = static_cast<std::size_t>(frame_index) * OutputChannelCount;
            if (!ExpectNear(stop_output[sample_index + 0], expected, 1e-6f, "stopped resolver loop should finish the current pass on the left channel"))
                return false;
            if (!ExpectNear(stop_output[sample_index + 1], expected, 1e-6f, "stopped resolver loop should finish the current pass on the right channel"))
                return false;
        }

        for (std::size_t sample_index = static_cast<std::size_t>(remaining_frames) * OutputChannelCount;
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
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 5 destroy fixture should compile", "phase 5 destroy fixture should load"))
            return false;

        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 8, "phase 5 destroy loop fixture should contain enough frames"))
            return false;

        rig.SetTag("player", "resolver.active");
        rig.SetValue("player", "speed", 1.0f);
        rig.Update();

        std::vector<float> startup_output(static_cast<std::size_t>(4) * OutputChannelCount);
        rig.Render(startup_output.data(), 4);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 1, "destroy test should start one bound loop"))
            return false;

        rig.DestroyEntity("player");
        rig.Update();

        const std::uint32_t remaining_frames = static_cast<std::uint32_t>(buffer.frame_count - 4);
        std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * OutputChannelCount);
        rig.Render(stop_output.data(), remaining_frames + 8);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 0, "DestroyEntity should stop every bound instance after its current pass"))
            return false;

        for (std::size_t sample_index = static_cast<std::size_t>(remaining_frames) * OutputChannelCount;
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
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 7 forwarding fixture should compile", "phase 7 forwarding fixture should load"))
            return false;

        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 4, "phase 7 fixture should contain enough frames"))
            return false;

        rig.SetValue("player", "volume", 0.5f);
        rig.SetPosition("player", 1.0f, 2.0f, 3.0f);
        rig.SetTag("player", "resolver.active");
        rig.Update();

        std::vector<float> startup_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(startup_output.data(), 1);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 1, "phase 7 match should create one bound instance"))
            return false;

        decl_audio::playback::InstanceSnapshot snapshot;
        constexpr decl_audio::playback::InstanceId kResolverInstanceId = 1;
        if (!Expect(rig.audio_runtime.TryGetInstanceSnapshot(kResolverInstanceId, snapshot), "phase 7 bound instance should expose a testing snapshot"))
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

        rig.SetValue("player", "volume", 0.25f);
        rig.SetPosition("player", 4.0f, 5.0f, 6.0f);
        rig.Update();

        std::vector<float> updated_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(updated_output.data(), 1);

        if (!Expect(rig.audio_runtime.TryGetInstanceSnapshot(kResolverInstanceId, snapshot), "phase 7 bound instance should remain active after parameter updates"))
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

    bool TestBlendLoopsStayInSyncAndRespondToParameterUpdates()
    {
        const std::filesystem::path fixture_path = GetFixturePath("NestedBehaviorBank.json");
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 10 blend fixture should compile", "phase 10 blend fixture should load"))
            return false;

        const decl_audio::compiler::ProgramId program_id = rig.compiled_bank.GetProgramId("nested.blend_loops");
        const decl_audio::compiler::ParameterId mix_parameter_id = rig.compiled_bank.GetParameterId("mix");
        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 2, "nested blend fixture should contain enough frames"))
            return false;

        constexpr decl_audio::playback::InstanceId kInstanceId = 8008;
        rig.SubmitAudioCommand(decl_audio::playback::CreateInstanceCommand{
            kInstanceId,
            program_id,
            Vec3{},
            1.0f});

        std::vector<float> startup_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(startup_output.data(), 1);

        const float expected_start = buffer.samples[0];
        if (!ExpectNear(startup_output[0], expected_start, 1e-6f, "blend should default to the first branch when the parameter is unset"))
            return false;
        if (!ExpectNear(startup_output[1], expected_start, 1e-6f, "blend should duplicate the default mono branch to the right channel"))
            return false;

        decl_audio::playback::DebugSnapshot snapshot = rig.audio_runtime.GetDebugSnapshot();
        if (!Expect(snapshot.active_instance_count == 1, "blend test should keep one instance active"))
            return false;
        if (!Expect(snapshot.instances[0].active_voice_count == 2, "blend should keep both child loops active"))
            return false;
        if (!Expect(snapshot.instances[0].nodes.size() == 4, "blend instance should expose root, blend, and two loop nodes"))
            return false;
        if (!Expect(snapshot.instances[0].voices.size() == 2, "blend instance should expose two active voices"))
            return false;
        if (!Expect(snapshot.instances[0].voices[0].sample_position == 1 && snapshot.instances[0].voices[1].sample_position == 1, "blend child loops should stay sample-aligned"))
            return false;

        rig.SubmitAudioCommand(decl_audio::playback::SetParameterCommand{
            kInstanceId,
            mix_parameter_id,
            1.0f});

        std::vector<float> updated_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(updated_output.data(), 1);

        const float expected_updated = buffer.samples[1] * 0.25f;
        if (!ExpectNear(updated_output[0], expected_updated, 1e-6f, "blend parameter updates should crossfade to the second branch on the next block"))
            return false;
        if (!ExpectNear(updated_output[1], expected_updated, 1e-6f, "blend parameter updates should affect both stereo channels"))
            return false;

        snapshot = rig.audio_runtime.GetDebugSnapshot();
        if (!Expect(snapshot.instances[0].voices[0].sample_position == 2 && snapshot.instances[0].voices[1].sample_position == 2, "blend children should remain in sync after parameter updates"))
            return false;

        return true;
    }

    bool TestSelectChoiceIsDeterministicAndOnlyEntersChosenSubtree()
    {
        const std::filesystem::path fixture_path = GetFixturePath("NestedBehaviorBank.json");
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 10 select fixture should compile", "phase 10 select fixture should load"))
            return false;

        const decl_audio::compiler::ProgramId program_id = rig.compiled_bank.GetProgramId("nested.select_blend_or_loop");
        constexpr decl_audio::playback::InstanceId kInstanceId = 8101;
        rig.SubmitAudioCommand(decl_audio::playback::CreateInstanceCommand{
            kInstanceId,
            program_id,
            Vec3{},
            1.0f});

        std::vector<float> output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(output.data(), 1);

        decl_audio::playback::DebugSnapshot snapshot = rig.audio_runtime.GetDebugSnapshot();
        if (!Expect(snapshot.active_instance_count == 1, "select test should create one instance"))
            return false;
        if (!Expect(snapshot.instances[0].nodes.size() == 6, "select instance should expose root, select, blend subtree, and alternate loop"))
            return false;

        const decl_audio::playback::InstanceDebugSnapshot &instance = snapshot.instances[0];
        const std::int32_t chosen_child = instance.nodes[1].chosen_child;
        if (!Expect(chosen_child == 0 || chosen_child == 1, "select should latch one authored child on entry"))
            return false;

        if (chosen_child == 0)
        {
            if (!Expect(instance.active_voice_count == 2, "selecting the blend branch should activate both blend loops"))
                return false;
            if (!Expect(instance.nodes[2].entered, "blend node should enter when selected"))
                return false;
            if (!Expect(instance.nodes[3].entered && instance.nodes[4].entered, "selected blend children should enter"))
                return false;
            if (!Expect(!instance.nodes[5].entered, "alternate loop should stay inactive when blend is selected"))
                return false;
        }
        else
        {
            if (!Expect(instance.active_voice_count == 1, "selecting the alternate loop should activate one voice"))
                return false;
            if (!Expect(!instance.nodes[2].entered && !instance.nodes[3].entered && !instance.nodes[4].entered, "unselected blend subtree should remain inactive"))
                return false;
            if (!Expect(instance.nodes[5].entered, "selected alternate loop should enter"))
                return false;
        }

        rig.SubmitAudioCommand(decl_audio::playback::SetParameterCommand{
            kInstanceId,
            rig.compiled_bank.GetParameterId("mix"),
            0.9f});
        rig.Render(output.data(), 1);

        snapshot = rig.audio_runtime.GetDebugSnapshot();
        if (!Expect(snapshot.instances[0].nodes[1].chosen_child == chosen_child, "select should not switch branches after entry"))
            return false;

        PlaybackTestRig rig_repeat;
        if (!rig_repeat.LoadFixture(fixture_path, "phase 10 repeated select fixture should compile", "phase 10 repeated select fixture should load"))
            return false;

        rig_repeat.SubmitAudioCommand(decl_audio::playback::CreateInstanceCommand{
            kInstanceId,
            program_id,
            Vec3{},
            1.0f});
        rig_repeat.Render(output.data(), 1);
        const decl_audio::playback::DebugSnapshot repeat_snapshot = rig_repeat.audio_runtime.GetDebugSnapshot();
        if (!Expect(repeat_snapshot.instances[0].nodes[1].chosen_child == chosen_child, "select choice should be deterministic for a stable seed"))
            return false;

        return true;
    }

    bool TestResolverForwardsDeclaredBlendParameter()
    {
        const std::filesystem::path fixture_path = GetFixturePath("NestedBehaviorBank.json");
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 10 resolver blend fixture should compile", "phase 10 resolver blend fixture should load"))
            return false;

        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 2, "resolver blend fixture should contain enough frames"))
            return false;

        rig.SetValue("player", "mix", 0.0f);
        rig.SetTag("player", "nested.active");
        rig.Update();

        std::vector<float> startup_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(startup_output.data(), 1);

        if (!ExpectNear(startup_output[0], buffer.samples[0], 1e-6f, "resolver should seed the initial blend parameter onto the first render block"))
            return false;
        if (!ExpectNear(startup_output[1], buffer.samples[0], 1e-6f, "resolver-seeded blend parameter should affect both channels"))
            return false;

        decl_audio::playback::DebugSnapshot snapshot = rig.audio_runtime.GetDebugSnapshot();
        if (!Expect(snapshot.instances[0].active_voice_count == 2, "resolver-driven blend should keep both loops active"))
            return false;

        rig.SetValue("player", "mix", 1.0f);
        rig.Update();

        std::vector<float> updated_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(updated_output.data(), 1);

        const float expected_updated = buffer.samples[1] * 0.25f;
        if (!ExpectNear(updated_output[0], expected_updated, 1e-6f, "resolver should forward declared parameters through SetParameter on the next block"))
            return false;
        if (!ExpectNear(updated_output[1], expected_updated, 1e-6f, "resolver-forwarded blend updates should affect both channels"))
            return false;

        return true;
    }

    bool TestSpatializedMonoRespondsToEntityAndListenerMovement()
    {
        const std::filesystem::path fixture_path = GetFixturePath("SpatializationBehaviorBank.json");
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 7.5 mono spatialization fixture should compile", "phase 7.5 mono spatialization fixture should load"))
            return false;

        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_1ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 2, "phase 7.5 mono fixture should contain enough frames"))
            return false;

        rig.SetListenerPosition(0.0f, 0.0f, 0.0f);
        rig.SetPosition("player", 3.0f, 0.0f, 0.0f);
        rig.SetTag("player", "spatial.mono");
        rig.Update();

        std::vector<float> startup_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(startup_output.data(), 1);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 1, "phase 7.5 mono match should create one bound instance"))
            return false;
        if (!Expect(rig.audio_runtime.GetListenerPositionForTesting() == Vec3{0.0f, 0.0f, 0.0f}, "listener position should forward through the control and audio chain"))
            return false;

        const StereoMixGains startup_gains = ComputeExpectedSpatialMix(Vec3{3.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f}, 1.0f, 5.0f);
        const float startup_sample = buffer.samples[0];
        if (!ExpectNear(startup_output[0], startup_sample * startup_gains.left, 1e-6f, "spatialized mono should pan and attenuate on the left channel"))
            return false;
        if (!ExpectNear(startup_output[1], startup_sample * startup_gains.right, 1e-6f, "spatialized mono should pan and attenuate on the right channel"))
            return false;

        rig.SetListenerPosition(4.0f, 0.0f, 0.0f);
        rig.Update();

        std::vector<float> updated_output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(updated_output.data(), 1);

        if (!Expect(rig.audio_runtime.GetListenerPositionForTesting() == Vec3{4.0f, 0.0f, 0.0f}, "listener movement should stick on the audio thread"))
            return false;

        const StereoMixGains updated_gains = ComputeExpectedSpatialMix(Vec3{3.0f, 0.0f, 0.0f}, Vec3{4.0f, 0.0f, 0.0f}, 1.0f, 5.0f);
        const float updated_sample = buffer.samples[1];
        if (!ExpectNear(updated_output[0], updated_sample * updated_gains.left, 1e-6f, "listener movement should update mono spatialization on the left channel"))
            return false;
        if (!ExpectNear(updated_output[1], updated_sample * updated_gains.right, 1e-6f, "listener movement should update mono spatialization on the right channel"))
            return false;

        return true;
    }

    bool TestSpatializedStereoAppliesBalanceAndAttenuation()
    {
        const std::filesystem::path fixture_path = GetFixturePath("SpatializationBehaviorBank.json");
        PlaybackTestRig rig;
        if (!rig.LoadFixture(fixture_path, "phase 7.5 stereo spatialization fixture should compile", "phase 7.5 stereo spatialization fixture should load"))
            return false;

        const decl_audio::compiler::ProgramId program_id = rig.compiled_bank.GetProgramId("spatial.stereo");
        const decl_audio::compiler::AssetId asset_id = rig.compiled_bank.GetAssetId("audio/test_48_24_2ch.wav");
        const decl_audio::assets::DecodedBuffer &buffer = rig.asset_bank.GetBuffer(asset_id);
        if (!Expect(buffer.frame_count > 1, "phase 7.5 stereo fixture should contain enough frames"))
            return false;

        constexpr decl_audio::playback::InstanceId kInstanceId = 7007;
        const Vec3 source_position{1.0f, 0.0f, 1.7320508f};
        rig.SubmitAudioCommand(decl_audio::playback::SetListenerPositionCommand{Vec3{0.0f, 0.0f, 0.0f}});
        rig.SubmitAudioCommand(decl_audio::playback::CreateInstanceCommand{
            kInstanceId,
            program_id,
            source_position,
            1.0f});

        std::vector<float> output(static_cast<std::size_t>(1) * OutputChannelCount);
        rig.Render(output.data(), 1);

        const StereoMixGains gains = ComputeExpectedSpatialMix(source_position, Vec3{0.0f, 0.0f, 0.0f}, 1.0f, 5.0f);
        if (!ExpectNear(output[0], buffer.samples[0] * gains.left, 1e-6f, "spatialized stereo should apply balance and attenuation to the left channel"))
            return false;
        if (!ExpectNear(output[1], buffer.samples[1] * gains.right, 1e-6f, "spatialized stereo should apply balance and attenuation to the right channel"))
            return false;

        return true;
    }

    bool TestCreateInstanceTerminatesOnCapacityExhaustion()
    {
        const char *test_executable_path = GetTestExecutablePath();
        if (!Expect(test_executable_path != nullptr && test_executable_path[0] != '\0', "death test requires the current test executable path"))
            return false;

        const std::filesystem::path executable_path = std::filesystem::absolute(test_executable_path);
        const std::filesystem::path started_flag_path = MakeDeathTestStartedFlagPath();
        std::error_code remove_error;
        std::filesystem::remove(started_flag_path, remove_error);

#if defined(_WIN32)
        const intptr_t raw_status = _spawnl(_P_WAIT,
                                            executable_path.string().c_str(),
                                            executable_path.string().c_str(),
                                            "--death-test-audio-capacity-overflow",
                                            started_flag_path.string().c_str(),
                                            nullptr);
        if (!Expect(raw_status != -1, "death test subprocess should launch"))
            return false;
        const int status = static_cast<int>(raw_status);
#else
        const pid_t child_pid = fork();
        if (!Expect(child_pid != -1, "death test subprocess should launch"))
            return false;

        if (child_pid == 0)
        {
            const int null_fd = open("/dev/null", O_WRONLY);
            if (null_fd >= 0)
            {
                dup2(null_fd, STDOUT_FILENO);
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);
            }

            execl(executable_path.string().c_str(),
                  executable_path.string().c_str(),
                  "--death-test-audio-capacity-overflow",
                  started_flag_path.string().c_str(),
                  static_cast<char *>(nullptr));
            _exit(127);
        }

        int status = 0;
        if (!Expect(waitpid(child_pid, &status, 0) == child_pid, "death test parent should wait for the child process"))
            return false;
#endif

        const bool child_started = std::filesystem::exists(started_flag_path);
        std::filesystem::remove(started_flag_path, remove_error);

        if (!Expect(child_started, "death test subprocess should reach the overflow setup path"))
            return false;
        if (!Expect(!ProcessExitedWithCode(status, kDeathTestUnexpectedSuccessExitCode), "capacity overflow should terminate, not return normally"))
            return false;
        if (!Expect(status != 0, "capacity overflow should not exit successfully"))
            return false;

        return true;
    }
} // namespace

int RunAudioCapacityOverflowDeathTestChild(const char *started_flag_path)
{
#if defined(_WIN32)
    SuppressWindowsAbortDialogsForDeathTest();
#endif

    std::ofstream started_flag(started_flag_path, std::ios::trunc);
    started_flag << "started";
    started_flag.close();

    const std::filesystem::path fixture_path = GetFixturePath("PlaybackBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);
    if (compile_result.HasErrors())
    {
        return 2;
    }

    const decl_audio::assets::LoadResult asset_result = decl_audio::assets::LoadAssetBank(compile_result.bank, fixture_path);
    if (asset_result.HasErrors())
    {
        return 3;
    }

    decl_audio::compiler::CompiledBank compiled_bank = compile_result.bank;
    decl_audio::assets::AssetBank asset_bank = asset_result.bank;
    decl_audio::playback::AudioRuntime audio_runtime(0xC0FFEEULL, 1, 8);
    audio_runtime.InstallBank(decl_audio::BankId{0u, 0u}, &compiled_bank, &asset_bank);

    const decl_audio::compiler::ProgramId program_id = compiled_bank.GetProgramId("playback.loop");
    audio_runtime.Submit(decl_audio::playback::CreateInstanceCommand{
        1,
        program_id,
        Vec3{},
        1.0f});
    audio_runtime.Submit(decl_audio::playback::CreateInstanceCommand{
        2,
        program_id,
        Vec3{},
        1.0f});

    float output[OutputChannelCount] = {};
    audio_runtime.Render(output, 1);
    return kDeathTestUnexpectedSuccessExitCode;
}

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

    if (!TestBlendLoopsStayInSyncAndRespondToParameterUpdates())
        return false;

    if (!TestSelectChoiceIsDeterministicAndOnlyEntersChosenSubtree())
        return false;

    if (!TestResolverForwardsDeclaredBlendParameter())
        return false;

    if (!TestSpatializedMonoRespondsToEntityAndListenerMovement())
        return false;

    if (!TestSpatializedStereoAppliesBalanceAndAttenuation())
        return false;

    if (!TestCreateInstanceTerminatesOnCapacityExhaustion())
        return false;

    std::cout << "Playback tests passed\n";
    return true;
}
