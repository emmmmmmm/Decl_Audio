#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "../src/assets/AssetBank.hpp"
#include "../src/backends/StubBackend.hpp"
#include "../src/compiler/Compiler.hpp"
#include "../src/playback/AudioRuntime.hpp"
#include "../src/runtime/BehaviorResolver.hpp"
#include "../src/runtime/ControlRuntime.hpp"

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

    struct StereoMixGains final
    {
        float left = 1.0f;
        float right = 1.0f;
    };

    struct PlaybackTestRig final
    {
        decl_audio::compiler::CompiledBank compiled_bank;
        decl_audio::assets::AssetBank asset_bank;
        decl_audio::runtime::ControlRuntime control_runtime;
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
            audio_runtime.SetBanks(&compiled_bank, &asset_bank);
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
                compiled_bank.GetTagId(tag)});
        }

        void RemoveTag(const char *entity_id, const char *tag)
        {
            control_runtime.Submit(decl_audio::runtime::RemoveTagCommand{
                std::string(entity_id),
                compiled_bank.GetTagId(tag)});
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
                compiled_bank.GetParameterId(parameter),
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

            behavior_resolver.Resolve(
                control_runtime.GetWorldState(),
                compiled_bank,
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
        std::vector<float> output(static_cast<std::size_t>(kFramesToRender) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        rig.Render(output.data(), kFramesToRender);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 1, "oneshot instance should still be active after a short render"))
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

        std::vector<float> warmup_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        rig.Render(warmup_output.data(), 1);

        rig.SubmitAudioCommand(decl_audio::playback::SetVolumeCommand{
            kInstanceId,
            0.5f});
        rig.SubmitAudioCommand(decl_audio::playback::SetPositionCommand{
            kInstanceId,
            Vec3{1.0f, 2.0f, 3.0f}});

        std::vector<float> output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
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

        std::vector<float> prefix_output(static_cast<std::size_t>(kPrefixFrames) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        rig.Render(prefix_output.data(), kPrefixFrames);

        rig.SubmitAudioCommand(decl_audio::playback::RequestStopCommand{kInstanceId});

        const std::uint32_t remaining_frames = static_cast<std::uint32_t>(buffer.frame_count - kPrefixFrames);
        std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        rig.Render(stop_output.data(), remaining_frames + 8);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 0, "RequestStop should retire the loop after its current pass"))
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

        std::vector<float> startup_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
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
        std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        rig.Render(stop_output.data(), remaining_frames + 8);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 0, "lost match should retire the bound instance after the current pass"))
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

        std::vector<float> startup_output(static_cast<std::size_t>(4) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        rig.Render(startup_output.data(), 4);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 1, "destroy test should start one bound loop"))
            return false;

        rig.DestroyEntity("player");
        rig.Update();

        const std::uint32_t remaining_frames = static_cast<std::uint32_t>(buffer.frame_count - 4);
        std::vector<float> stop_output(static_cast<std::size_t>(remaining_frames + 8) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        rig.Render(stop_output.data(), remaining_frames + 8);

        if (!Expect(rig.audio_runtime.ActiveInstanceCount() == 0, "DestroyEntity should stop every bound instance after its current pass"))
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

        std::vector<float> startup_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
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

        std::vector<float> updated_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
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

        std::vector<float> startup_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
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

        std::vector<float> updated_output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
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

        std::vector<float> output(static_cast<std::size_t>(1) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        rig.Render(output.data(), 1);

        const StereoMixGains gains = ComputeExpectedSpatialMix(source_position, Vec3{0.0f, 0.0f, 0.0f}, 1.0f, 5.0f);
        if (!ExpectNear(output[0], buffer.samples[0] * gains.left, 1e-6f, "spatialized stereo should apply balance and attenuation to the left channel"))
            return false;
        if (!ExpectNear(output[1], buffer.samples[1] * gains.right, 1e-6f, "spatialized stereo should apply balance and attenuation to the right channel"))
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

    if (!TestSpatializedMonoRespondsToEntityAndListenerMovement())
        return false;

    if (!TestSpatializedStereoAppliesBalanceAndAttenuation())
        return false;

    std::cout << "Playback tests passed\n";
    return true;
}
