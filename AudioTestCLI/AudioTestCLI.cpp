#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <functional>
#include <sstream>
#include <chrono>

#include "SoundManagerAPI.hpp"
#include "AudioBehavior.hpp" 
#include "BehaviorLoader.hpp"
#include "Log.hpp"
#include <SoundManager.hpp>


// TODO: Those funcs no longer work!
// SoundManager_GetLastEmitName() currently returns the behaviorID, which we don't know here. 
// -> this needs some rework
/*
void AssertEmitted(SoundManager* mgr, int expectedCount) {
	int actualCount = SoundManager_GetLastEmitCount(mgr);
	if (actualCount != expectedCount) {
		std::cerr << "[ASSERT FAILED] Expected " << expectedCount << " events, but got " << actualCount << "." << std::endl;
		//exit(1);  // You could also return or throw depending on how you want to handle this
	}
	std::cout << "[ASSERT PASS] " << expectedCount << " events emitted." << std::endl;
}
void AssertEmittedSound(SoundManager* mgr, const std::string& expectedSound, int index = 0) {
	std::string actualSound = SoundManager_GetLastEmitName(mgr, index);
	if (actualSound != expectedSound) {
		std::cerr << "[ASSERT FAILED] Expected sound: " << expectedSound << ", but got: " << actualSound << std::endl;
		//exit(1);
	}
	std::cout << "[ASSERT PASS] Expected sound: " << expectedSound << " matched." << std::endl;
}
*/

void RunBasicBehaviorTest() {

	LogMessage("=== Basic Behavior Test ===", LogCategory::CLI, LogLevel::Info);

	AudioConfig cfg;
	cfg.bufferFrames = 512;
	cfg.channels = 1;
	cfg.sampleRate = 44100;
	cfg.backend = AudioBackend::Miniaudio;
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager(&cfg));

	SoundManager_SetAssetPath(mgr, "C:/Users/manuel/source/repos/DeclarativeSoundEngine/x64/Debug/");
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");

	SoundManager_SetTag(mgr, "player", "entity.player");
	SoundManager_SetTag(mgr, "player", "foot.contact.left");

	SoundManager_SetValue(mgr, "player", "velocity", 3.2f);
	SoundManager_SetValue(mgr, "player", "fatigue", 0.8f);
	SoundManager_SetGlobalValue(mgr, "time", 0.5f);



	int count = 0;
	auto start = std::chrono::steady_clock::now();
	while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
		SoundManager_Update(mgr);      // enqueues new commands
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
		count++;

		if (count == 10)
			SoundManager_ClearTag(mgr, "player", "foot.contact.left");

		if (count == 50) {
			SoundManager_SetTransientTag(mgr, "player", "foot.contact.left");
		}
		// now auto-cleared via transient tag
		//if (count == 55) {
		//	SoundManager_ClearTag(mgr, "player", "foot.contact.left");
		//}

		if (count == 100) {
			SoundManager_SetTag(mgr, "player", "foot.contact.right");
		}
		if (count == 105) {
			SoundManager_ClearTag(mgr, "player", "foot.contact.right");
		}
	}

	SoundManager_DebugPrintState(mgr);

	SoundManager_ClearTag(mgr, "player", "foot.contact.left");
	SoundManager_ClearTag(mgr, "player", "entity.player");
	SoundManager_ClearValue(mgr, "player", "velocity");
	SoundManager_ClearEntity(mgr, "monster");

	SoundManager_DebugPrintState(mgr);

	DestroySoundManager(mgr);
}


std::queue<std::function<void()>> gCommands;
std::mutex                  gCmdMutex;

void EnqueueCommand(std::function<void()> cmd) {
	std::lock_guard<std::mutex> lg(gCmdMutex);
	gCommands.push(std::move(cmd));
}

void ProcessCommands(SoundManager* mgr) {
	std::lock_guard<std::mutex> lg(gCmdMutex);
	while (!gCommands.empty()) {
		gCommands.front()();
		gCommands.pop();
	}
}

void RunInteractiveTest() {

	AudioConfig cfg;
	cfg.bufferFrames = 512;
	cfg.channels = 2;
	cfg.sampleRate = 44100;
	cfg.backend = AudioBackend::Miniaudio;
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager(&cfg));
	SoundManager_SetAssetPath(mgr, "C:/Users/manuel/source/repos/DeclarativeSoundEngine/x64/Debug/");
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");

	std::atomic<bool> running{ true };



	SoundManager_SetTag(mgr, "l", "listener");
	SoundManager_SetPosition(mgr, "l", 0, 0, 0);
	SoundManager_SetTag(mgr, "a", "entity.player");
	SoundManager_SetPosition(mgr, "a", -5, 0, 0);
	SoundManager_SetGlobalTag(mgr, "gamestate.ready");
	SoundManager_SetGlobalValue(mgr, "difficulty", 1);

		


	// Input thread: blocks on getline, parses lines, enqueues actions
	std::thread inputThread([&]() {
		std::string line;
		while (running && std::getline(std::cin, line)) {


			std::istringstream iss(line);
			std::string cmd; iss >> cmd;
			if (cmd == "exit") {
				running = false;
				break;
			}
			else if (cmd == "tag") {
				std::string entity, tag;
				iss >> entity >> tag;
				EnqueueCommand([=]() {
					SoundManager_SetTag(mgr, entity.c_str(), tag.c_str());
					});
			}
			else if (cmd == "clear") {
				std::string entity, tag;
				iss >> entity >> tag;
				EnqueueCommand([=]() {
					SoundManager_ClearTag(mgr, entity.c_str(), tag.c_str());
					});
			}
			else if (cmd == "transient") {
				std::string entity, tag;
				iss >> entity >> tag;
				EnqueueCommand([=]() {
					SoundManager_SetTransientTag(mgr, entity.c_str(), tag.c_str());
					});
			}
			else if (cmd == "value") {
				std::string entity, key; float v;
				iss >> entity >> key >> v;
				EnqueueCommand([=]() {
					SoundManager_SetValue(mgr, entity.c_str(), key.c_str(), v);
					});
			}
			else if (cmd == "pos") {
				std::string entity; float x, y, z;
				iss >> entity >> x >> y >> z;
				EnqueueCommand([=]() {
					SoundManager_SetPosition(mgr, entity.c_str(), x, y, z);
					});

			}
			else if (cmd == "log") {
				EnqueueCommand([=]() {
					SoundManager_DebugPrintState(mgr);
					});
			}
			else {
				std::cout << "Unknown command. Use:\n"
					"  tag <entity> <tag>\n"
					"  clear <entity> <tag>\n"
					"  transient <entity> <tag>\n"
					"  value <entity> <key> <float>\n"
					"  pos <entity> <x> <y> <z>\n"
					"  log\n"
					"  exit\n";
			}
		}
		});

	// Main update loop
	auto last = std::chrono::steady_clock::now();
	while (running) {
		auto now = std::chrono::steady_clock::now();
		if (now - last >= std::chrono::milliseconds(16)) {
			ProcessCommands(mgr);
			SoundManager_Update(mgr);
			last = now;
		}
		// sleep a bit to avoid busy-spin
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	inputThread.join();
	DestroySoundManager(mgr);
}




void RunValueConditionTests() {
	LogMessage("=== Value Condition Tests ===", LogCategory::CLI, LogLevel::Info);

	AudioConfig cfg;
	cfg.bufferFrames = 512;
	cfg.channels = 2;
	cfg.sampleRate = 44100;
	cfg.backend = AudioBackend::Miniaudio;

	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager(&cfg));
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");
	SoundManager_SetTag(mgr, "player", "entity.player");

	SoundManager_SetValue(mgr, "player", "velocity", 0.0f); // idle
	SoundManager_Update(mgr);

	//AssertEmittedSound(mgr, "idle.wav");  // Assert that "idle.wav" was triggered


	SoundManager_SetValue(mgr, "player", "velocity", 2.0f); // walk
	SoundManager_Update(mgr);

	//AssertEmittedSound(mgr, "walk.wav");

	SoundManager_SetValue(mgr, "player", "velocity", 3.5f); // run
	SoundManager_Update(mgr);

	//AssertEmittedSound(mgr, "run.wav");

	DestroySoundManager(mgr);
}

void RunInvalidTagTest() {
	LogMessage("=== Invalid Tag Test ===", LogCategory::CLI, LogLevel::Info);
	AudioConfig cfg;
	cfg.bufferFrames = 512;
	cfg.channels = 2;
	cfg.sampleRate = 44100;
	cfg.backend = AudioBackend::Miniaudio;
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager(&cfg));
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");
	SoundManager_SetTag(mgr, "ghost", "nonexistent.tag");
	SoundManager_SetTag(mgr, "ghost", "nonexistent2..tag");
	SoundManager_Update(mgr);
	DestroySoundManager(mgr);
}

void RunWildcardMatchingTest() {
	LogMessage("=== Wildcard Matching Test ===", LogCategory::CLI, LogLevel::Info);
	AudioConfig cfg;
	cfg.bufferFrames = 512;
	cfg.channels = 2;
	cfg.sampleRate = 44100;
	cfg.backend = AudioBackend::Miniaudio;
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager(&cfg));
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");
	SoundManager_SetTag(mgr, "npc", "entity.npc.guard");
	SoundManager_SetTag(mgr, "npc", "foot.contact.left");
	SoundManager_Update(mgr);
	DestroySoundManager(mgr);
}

void RunNoMatchTest() {
	LogMessage("=== No Match Test ===", LogCategory::CLI, LogLevel::Info);
	AudioConfig cfg;
	cfg.bufferFrames = 512;
	cfg.channels = 2;
	cfg.sampleRate = 44100;
	cfg.backend = AudioBackend::Miniaudio;
	SoundManager* mgr = static_cast<SoundManager*>(CreateSoundManager(&cfg));
	SoundManager_LoadBehaviorsFromFile(mgr, "./behaviors");
	SoundManager_SetTag(mgr, "thing", "something.unknown");
	SoundManager_Update(mgr);

	DestroySoundManager(mgr);
}


int main() {

	RunInteractiveTest(); // <- this. is. so. cool! xD

	//RunBasicBehaviorTest();
	//RunValueConditionTests();
	//RunInvalidTagTest();
	//RunWildcardMatchingTest();
	//RunNoMatchTest();

	return 0;
}


