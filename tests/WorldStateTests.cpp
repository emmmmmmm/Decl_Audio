#include <filesystem>
#include <iostream>
#include <vector>

#include "../include/Decl_Audio/Decl_Audio.h"
#include "../src/compiler/Compiler.hpp"
#include "../src/core/Engine.hpp"
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

    void RenderAudioForTesting(decl_audio::Engine &engine, const std::uint32_t frames)
    {
        std::vector<float> output(static_cast<std::size_t>(frames) * decl_audio::playback::AudioRuntime::OutputChannelCount);
        engine.RenderAudioForTesting(output.data(), frames);
    }

    bool TestCommandsDrainIntoWorldState()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 2 fixture should load"))
        {
            return false;
        }

        engine.SetTag("player", "movement.grounded");
        engine.SetTag("player", "movement.walking");
        engine.SetValue("player", "speed", 4.2f);

        if (!Expect(!engine.GetWorldState().HasEntity("player"), "queued commands should wait until Update"))
        {
            return false;
        }

        engine.Update();

        const decl_audio::runtime::WorldState &world_state = engine.GetWorldState();
        const decl_audio::compiler::TagId grounded_tag_id = compile_result.bank.GetTagId("movement.grounded");
        const decl_audio::compiler::TagId walking_tag_id = compile_result.bank.GetTagId("movement.walking");
        const decl_audio::compiler::ParameterId speed_parameter_id = compile_result.bank.GetParameterId("speed");
        if (!Expect(world_state.HasEntity("player"), "Update should materialize the entity"))
        {
            return false;
        }

        const decl_audio::runtime::EntityState &entity = world_state.GetEntity("player");
        if (!Expect(entity.HasTag(grounded_tag_id), "grounded tag should be applied"))
        {
            return false;
        }

        if (!Expect(entity.HasTag(walking_tag_id), "walking tag should be applied"))
        {
            return false;
        }

        if (!Expect(entity.GetFloatValue(speed_parameter_id) == 4.2f, "float value should be applied"))
        {
            return false;
        }

        return true;
    }

    bool TestMissedUpdatesDelayButDoNotDropCommands()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "missed-update fixture should load"))
        {
            return false;
        }

        engine.SetTag("player", "movement.grounded");
        engine.SetValue("player", "speed", 2.0f);

        if (!Expect(!engine.GetWorldState().HasEntity("player"), "missed Update should leave queued commands pending"))
        {
            return false;
        }

        engine.SetTag("player", "movement.walking");
        engine.SetPosition("player", 1.0f, 2.0f, 3.0f);

        if (!Expect(!engine.GetWorldState().HasEntity("player"), "additional queued commands should still wait for Update"))
        {
            return false;
        }

        engine.Update();

        const decl_audio::runtime::EntityState &entity = engine.GetWorldState().GetEntity("player");
        const decl_audio::compiler::TagId grounded_tag_id = compile_result.bank.GetTagId("movement.grounded");
        const decl_audio::compiler::TagId walking_tag_id = compile_result.bank.GetTagId("movement.walking");
        const decl_audio::compiler::ParameterId speed_parameter_id = compile_result.bank.GetParameterId("speed");
        if (!Expect(entity.HasTag(grounded_tag_id), "delayed Update should still apply grounded tags"))
        {
            return false;
        }

        if (!Expect(entity.HasTag(walking_tag_id), "delayed Update should still apply walking tags"))
        {
            return false;
        }

        if (!Expect(entity.GetFloatValue(speed_parameter_id) == 2.0f, "delayed Update should still apply parameter writes"))
        {
            return false;
        }

        if (!Expect(entity.HasPosition(), "delayed Update should still apply position writes"))
        {
            return false;
        }

        if (!Expect(entity.GetPosition() == Vec3{1.0f, 2.0f, 3.0f}, "delayed Update should preserve the latest queued position"))
        {
            return false;
        }

        return true;
    }

    bool TestRemoveTagAndDestroyEntity()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 2 removal fixture should load"))
        {
            return false;
        }

        engine.SetTag("player", "movement.grounded");
        engine.SetTag("player", "movement.walking");
        engine.SetValue("player", "speed", 1.0f);
        engine.Update();

        engine.RemoveTag("player", "movement.walking");
        engine.Update();

        const decl_audio::runtime::EntityState &entity = engine.GetWorldState().GetEntity("player");
        const decl_audio::compiler::TagId grounded_tag_id = compile_result.bank.GetTagId("movement.grounded");
        const decl_audio::compiler::TagId walking_tag_id = compile_result.bank.GetTagId("movement.walking");
        if (!Expect(entity.HasTag(grounded_tag_id), "removing one tag should keep other tags"))
        {
            return false;
        }

        if (!Expect(!entity.HasTag(walking_tag_id), "RemoveTag should erase the requested tag"))
        {
            return false;
        }

        engine.DestroyEntity("player");
        engine.Update();

        if (!Expect(!engine.GetWorldState().HasEntity("player"), "DestroyEntity should erase the entity"))
        {
            return false;
        }

        return true;
    }

    bool TestRemoveCommandsDoNotCreateEntities()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 2 remove fixture should load"))
        {
            return false;
        }

        engine.RemoveTag("ghost", "movement.walking");
        engine.DestroyEntity("ghost");
        engine.Update();

        if (!Expect(!engine.GetWorldState().HasEntity("ghost"), "remove-style commands should not create entities"))
        {
            return false;
        }

        return true;
    }

    bool TestTransientTagsExpireWithoutErasingPersistentTags()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);
        const decl_audio::compiler::TagId grounded_tag_id = compile_result.bank.GetTagId("movement.grounded");

        decl_audio::runtime::ControlRuntime control_runtime;
        control_runtime.Submit(decl_audio::runtime::SetTransientTagCommand{
            "player",
            grounded_tag_id});
        control_runtime.Tick();

        if (!Expect(control_runtime.GetWorldState().HasEntity("player"), "transient tag should materialize the entity during drain"))
        {
            return false;
        }

        const decl_audio::runtime::EntityState &transient_entity = control_runtime.GetWorldState().GetEntity("player");
        if (!Expect(transient_entity.HasTag(grounded_tag_id), "transient tag should be visible before cleanup"))
        {
            return false;
        }

        control_runtime.ClearTransientTags();

        const decl_audio::runtime::EntityState &cleared_entity = control_runtime.GetWorldState().GetEntity("player");
        if (!Expect(!cleared_entity.HasTag(grounded_tag_id), "ClearTransientTags should remove transient tags"))
        {
            return false;
        }

        control_runtime.Submit(decl_audio::runtime::SetTagCommand{
            "player",
            grounded_tag_id});
        control_runtime.Submit(decl_audio::runtime::SetTransientTagCommand{
            "player",
            grounded_tag_id});
        control_runtime.Tick();
        control_runtime.ClearTransientTags();

        const decl_audio::runtime::EntityState &persistent_entity = control_runtime.GetWorldState().GetEntity("player");
        if (!Expect(persistent_entity.HasTag(grounded_tag_id), "clearing transient tags should not erase persistent tags"))
        {
            return false;
        }

        return true;
    }

    bool TestEngineTransientTagsDriveExactlyOneResolverPass()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ParameterForwardingBehaviorBank.json");
        const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);
        const decl_audio::compiler::TagId resolver_tag_id = compile_result.bank.GetTagId("resolver.active");

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "transient resolver fixture should load"))
        {
            return false;
        }

        engine.SetTransientTag("player", "resolver.active");
        engine.Update();

        if (!Expect(engine.GetWorldState().HasEntity("player"), "transient Update should still materialize the entity"))
        {
            return false;
        }

        const decl_audio::runtime::EntityState &entity = engine.GetWorldState().GetEntity("player");
        if (!Expect(!entity.HasTag(resolver_tag_id), "Update should clear transient tags before returning"))
        {
            return false;
        }

        decl_audio::playback::DebugSnapshot snapshot = engine.GetDebugSnapshot();
        if (!Expect(snapshot.active_instance_count == 0, "resolver output should remain queued until audio renders"))
        {
            return false;
        }

        RenderAudioForTesting(engine, 1);

        snapshot = engine.GetDebugSnapshot();
        if (!Expect(snapshot.active_instance_count == 1, "transient tag should create an instance on the next render block"))
        {
            return false;
        }

        if (!Expect(!snapshot.instances[0].stop_requested, "fresh transient match should not request stop before the next Update"))
        {
            return false;
        }

        engine.Update();

        snapshot = engine.GetDebugSnapshot();
        if (!Expect(!snapshot.instances[0].stop_requested, "resolver stop should remain queued until the next render block"))
        {
            return false;
        }

        RenderAudioForTesting(engine, 1);

        snapshot = engine.GetDebugSnapshot();
        if (!Expect(snapshot.instances[0].stop_requested, "transient tag should stop matching after one Update pass"))
        {
            return false;
        }

        return true;
    }

    bool TestVec3CommandsDrainIntoWorldState()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ParameterForwardingBehaviorBank.json");

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "phase 7 fixture should load"))
        {
            return false;
        }

        engine.SetPosition("player", 1.0f, 2.0f, 3.0f);
        engine.Update();

        const decl_audio::runtime::EntityState &entity = engine.GetWorldState().GetEntity("player");
        if (!Expect(entity.HasPosition(), "SetPosition should mark the entity as having a runtime position"))
        {
            return false;
        }

        if (!Expect(entity.GetPosition() == Vec3{1.0f, 2.0f, 3.0f}, "position value should be applied"))
        {
            return false;
        }

        return true;
    }

    bool TestResolverChangesApplyOnNextRenderBlock()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ParameterForwardingBehaviorBank.json");

        auto config = GetTestConfig();

        decl_audio::Engine engine(config);
        if (!Expect(engine.LoadBehaviors(fixture_path.string().c_str()), "next-block fixture should load"))
        {
            return false;
        }

        engine.SetValue("player", "volume", 0.5f);
        engine.SetTag("player", "resolver.active");
        engine.Update();

        decl_audio::playback::DebugSnapshot snapshot = engine.GetDebugSnapshot();
        if (!Expect(snapshot.active_instance_count == 0, "resolver create should stay queued until render"))
        {
            return false;
        }

        RenderAudioForTesting(engine, 1);

        snapshot = engine.GetDebugSnapshot();
        if (!Expect(snapshot.active_instance_count == 1, "first render block should apply resolver-created instances"))
        {
            return false;
        }

        if (!Expect(snapshot.instances[0].volume == 0.5f, "created instance should pick up the first resolved volume"))
        {
            return false;
        }

        engine.SetValue("player", "volume", 0.25f);
        engine.Update();

        snapshot = engine.GetDebugSnapshot();
        if (!Expect(snapshot.instances[0].volume == 0.5f, "resolved volume changes should remain queued until render"))
        {
            return false;
        }

        RenderAudioForTesting(engine, 1);

        snapshot = engine.GetDebugSnapshot();
        if (!Expect(snapshot.instances[0].volume == 0.25f, "resolved volume changes should apply on the next render block"))
        {
            return false;
        }

        return true;
    }

    bool TestPhase2ApiSmoke()
    {
        const std::filesystem::path fixture_path = GetFixturePath("ParameterForwardingBehaviorBank.json");

        auto config = GetTestConfig();

        DeclAudioEngine *engine = nullptr;
        if (!Expect(CreateEngine(&config, &engine), "CreateEngine should succeed for the phase 2 API smoke test"))
        {
            return false;
        }

        if (!Expect(LoadBehaviors(engine, fixture_path.string().c_str()), "LoadBehaviors should succeed for the phase 2 API smoke test"))
        {
            DestroyEngine(engine);
            return false;
        }

        SetTag(engine, "player", "resolver.active");
        SetValue(engine, "player", "volume", 0.5f);
        SetPosition(engine, "player", 1.0f, 2.0f, 3.0f);
        Update(engine);
        RemoveTag(engine, "player", "resolver.active");
        Update(engine);
        DestroyEntity(engine, "player");
        Update(engine);

        DestroyEngine(engine);
        return true;
    }
} // namespace

bool RunWorldStateTests()
{
    if (!TestCommandsDrainIntoWorldState())
    {
        return false;
    }

    if (!TestRemoveTagAndDestroyEntity())
    {
        return false;
    }

    if (!TestMissedUpdatesDelayButDoNotDropCommands())
    {
        return false;
    }

    if (!TestRemoveCommandsDoNotCreateEntities())
    {
        return false;
    }

    if (!TestTransientTagsExpireWithoutErasingPersistentTags())
    {
        return false;
    }

    if (!TestEngineTransientTagsDriveExactlyOneResolverPass())
    {
        return false;
    }

    if (!TestPhase2ApiSmoke())
    {
        return false;
    }

    if (!TestVec3CommandsDrainIntoWorldState())
    {
        return false;
    }

    if (!TestResolverChangesApplyOnNextRenderBlock())
    {
        return false;
    }

    std::cout << "WorldState tests passed\n";
    return true;
}
