#include <iostream>

#include "SoundManagerAPI.hpp"
#include "AudioBehavior.hpp"
#include "BehaviorLoader.hpp"
#include "Log.hpp"

void AssertEmitted(SoundManager* mgr, int expectedCount) {
    int actualCount = SoundManager_GetLastEmitCount(mgr);
    if (actualCount != expectedCount) {
        std::cerr << "[ASSERT FAILED] Expected " << expectedCount << " events, but got " << actualCount << "." << std::endl;
        exit(1);  // You could also return or throw depending on how you want to handle this
    }
    std::cout << "[ASSERT PASS] " << expectedCount << " events emitted." << std::endl;
}

void AssertEmittedSound(SoundManager* mgr, const std::string& expectedSound, int index = 0) {
    std::string actualSound = SoundManager_GetLastEmitName(mgr, index);
    if (actualSound != expectedSound) {
        std::cerr << "[ASSERT FAILED] Expected sound: " << expectedSound << ", but got: " << actualSound << std::endl;
        exit(1);
    }
    std::cout << "[ASSERT PASS] Expected sound: " << expectedSound << " matched." << std::endl;
}



void RunBasicBehaviorTest() {
    LogMessage("=== Basic Behavior Test ===", LogCategory::CLI, LogLevel::Info);
    SoundManager* mgr = CreateSoundManager();
    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");

    SoundManager_SetTag(mgr, "player", "entity.player");
    SoundManager_SetTag(mgr, "player", "foot.leftContact");
    SoundManager_SetTag(mgr, "monster", "entity.monster");
    SoundManager_SetTag(mgr, "monster", "foot.leftContact");

    SoundManager_SetValue(mgr, "player", "velocity", 3.2f);
    SoundManager_SetValue(mgr, "player", "fatigue", 0.8f);
    SoundManager_SetGlobalValue(mgr, "time", 0.5f);

    SoundManager_Update(mgr);
    SoundManager_DebugPrintState(mgr);

    SoundManager_ClearTag(mgr, "player", "foot.leftContact");
    SoundManager_ClearTag(mgr, "player", "entity.player");
    SoundManager_ClearValue(mgr, "player", "velocity");
    SoundManager_ClearEntity(mgr, "monster");

    SoundManager_DebugPrintState(mgr);


    DestroySoundManager(mgr);
}

void RunValueConditionTests() {
    LogMessage("=== Value Condition Tests ===", LogCategory::CLI, LogLevel::Info);
    SoundManager* mgr = CreateSoundManager();
    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");
    SoundManager_SetTag(mgr, "player", "entity.player");

    SoundManager_SetValue(mgr, "player", "velocity", 0.0f); // idle
    SoundManager_Update(mgr);
    AssertEmittedSound(mgr, "idle.wav");  // Assert that "idle.wav" was triggered

    SoundManager_SetValue(mgr, "player", "velocity", 2.0f); // walk
    SoundManager_Update(mgr);

    AssertEmittedSound(mgr, "walk.wav");

    SoundManager_SetValue(mgr, "player", "velocity", 3.5f); // run
    SoundManager_Update(mgr);

    AssertEmittedSound(mgr, "run.wav");

    DestroySoundManager(mgr);
}

void RunInvalidTagTest() {
    LogMessage("=== Invalid Tag Test ===", LogCategory::CLI, LogLevel::Info);
    SoundManager* mgr = CreateSoundManager();
    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");
    SoundManager_SetTag(mgr, "ghost", "nonexistent.tag");
    SoundManager_SetTag(mgr, "ghost", "nonexistent2..tag");
    SoundManager_Update(mgr);
    DestroySoundManager(mgr);
}

void RunWildcardMatchingTest() {
    LogMessage("=== Wildcard Matching Test ===", LogCategory::CLI, LogLevel::Info);
    SoundManager* mgr = CreateSoundManager();
    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");
    SoundManager_SetTag(mgr, "npc", "entity.npc.guard");
    SoundManager_SetTag(mgr, "npc", "foot.leftContact");
    SoundManager_Update(mgr);
    DestroySoundManager(mgr);
}

void RunNoMatchTest() {
    LogMessage("=== No Match Test ===", LogCategory::CLI, LogLevel::Info);
    SoundManager* mgr = CreateSoundManager();
    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");
    SoundManager_SetTag(mgr, "thing", "something.unknown");
    SoundManager_Update(mgr);

    
    DestroySoundManager(mgr);
}


int main() {
    RunBasicBehaviorTest();
    //RunValueConditionTests();
    //RunInvalidTagTest();
    //RunWildcardMatchingTest();
    //RunNoMatchTest();
    return 0;
}


