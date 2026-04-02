#include <filesystem>
#include <iostream>

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

std::filesystem::path GetFixturePath(const char *file_name)
{
    return std::filesystem::path(__FILE__).parent_path() / "data" / file_name;
}

bool TestCommandsDrainIntoWorldState()
{
    const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

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

bool TestRemoveTagAndDestroyEntity()
{
    const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");
    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(fixture_path);

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

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

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

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

bool TestPhase2ApiSmoke()
{
    const std::filesystem::path fixture_path = GetFixturePath("ValidBehaviorBank.json");

    EngineConfig config{};
    config.struct_size = sizeof(EngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;

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

    SetTag(engine, "player", "movement.grounded");
    SetTag(engine, "player", "movement.walking");
    SetValue(engine, "player", "speed", 2.0f);
    Update(engine);
    RemoveTag(engine, "player", "movement.walking");
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

    if (!TestRemoveCommandsDoNotCreateEntities())
    {
        return false;
    }

    if (!TestPhase2ApiSmoke())
    {
        return false;
    }

    std::cout << "WorldState tests passed\n";
    return true;
}
