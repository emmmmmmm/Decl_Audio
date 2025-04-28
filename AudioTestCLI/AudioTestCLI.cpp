#include <iostream>
#include <chrono>

// why can we access those classes at all?
#include "SoundManagerAPI.hpp"
#include "AudioBehavior.hpp" 
#include "BehaviorLoader.hpp"
#include "Log.hpp"
#include <SoundManager.hpp>

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

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();


	LogMessage("=== Basic Behavior Test ===", LogCategory::CLI, LogLevel::Info);
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager());
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");

	SoundManager_SetTag(mgr, "player", "entity.player");
	SoundManager_SetTag(mgr, "player", "foot.contact.left");
	SoundManager_SetTag(mgr, "monster", "entity.monster");
	SoundManager_SetTag(mgr, "monster", "foot.contact.left");

	SoundManager_SetValue(mgr, "player", "velocity", 3.2f);
	SoundManager_SetValue(mgr, "player", "fatigue", 0.8f);
	SoundManager_SetGlobalValue(mgr, "time", 0.5f);
	SoundManager_Update(mgr);
	SoundManager_DebugPrintState(mgr);

	SoundManager_ClearTag(mgr, "player", "foot.contact.left");
	SoundManager_ClearTag(mgr, "player", "entity.player");
	SoundManager_ClearValue(mgr, "player", "velocity");
	SoundManager_ClearEntity(mgr, "monster");

	SoundManager_DebugPrintState(mgr);


	DestroySoundManager(mgr);
	// thats ... not fast -,-
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "Test took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms" << std::endl;

}

void RunValueConditionTests() {
	LogMessage("=== Value Condition Tests ===", LogCategory::CLI, LogLevel::Info);
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager());
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");
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
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager());
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");
	SoundManager_SetTag(mgr, "ghost", "nonexistent.tag");
	SoundManager_SetTag(mgr, "ghost", "nonexistent2..tag");
	SoundManager_Update(mgr);
	DestroySoundManager(mgr);
}

void RunWildcardMatchingTest() {
	LogMessage("=== Wildcard Matching Test ===", LogCategory::CLI, LogLevel::Info);
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager());
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");
	SoundManager_SetTag(mgr, "npc", "entity.npc.guard");
	SoundManager_SetTag(mgr, "npc", "foot.contact.left");
	SoundManager_Update(mgr);
	DestroySoundManager(mgr);
}

void RunNoMatchTest() {
	LogMessage("=== No Match Test ===", LogCategory::CLI, LogLevel::Info);
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager());
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");
	SoundManager_SetTag(mgr, "thing", "something.unknown");
	SoundManager_Update(mgr);

	DestroySoundManager(mgr);
}
void RunBufferTest() {

	LogMessage("=== Buffer Test ===", LogCategory::CLI, LogLevel::Info);
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager());	
	SoundManager_BufferTest(mgr);
	DestroySoundManager(mgr);
}

int main() {
	RunBasicBehaviorTest();
	//RunValueConditionTests();
	//RunInvalidTagTest();
	//RunWildcardMatchingTest();
	//RunNoMatchTest();

	RunBufferTest();

	return 0;
}


