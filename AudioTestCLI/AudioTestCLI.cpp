#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <functional>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <string>

#include "BehaviorDef.hpp" 
#include "BehaviorLoader.hpp"
#include "Log.hpp"
#include <AudioManager.hpp>
#include "AudioManagerAPI.hpp"


using Clock = std::chrono::steady_clock;

const char* assetPath = "C:/Users/manuel/source/repos/DeclarativeSoundEngine/x64/Debug/";
const char* behaviorPath = "C:/Users/manuel/source/repos/DeclarativeSoundEngine/x64/Debug/behaviors";


static AudioConfig GetTestConfig() {
        AudioConfig cfg{};
        cfg.bufferFrames = 512;
        cfg.channels = 2;
        cfg.sampleRate = 48000;
        cfg.backend = AudioBackend::Miniaudio;

        const char* env = std::getenv("DECLSOUND_BACKEND");
        if (env) {
                std::string v = env;
                if (v == "stub") cfg.backend = AudioBackend::Stub;
                else if (v == "unity") cfg.backend = AudioBackend::Unity;
        }
        return cfg;
}


static void RunBlendNodeTest() {
	LogMessage("=== Blend Node Test ===", LogCategory::CLI, LogLevel::Info);

	auto cfg = GetTestConfig();
	AudioManager_Create(&cfg);
	AudioManager_LoadBehaviorsFromFile(behaviorPath, assetPath);
	AudioManager_SetTag("l", "listener");
	AudioManager_SetVectorValue("l", "position", 0, 0, 0);
	AudioManager_SetTag("player", "entity.tester");
	AudioManager_SetVectorValue("player", "position", 5, 0, 0);
	AudioManager_SetTag("player", "test.blend");


	auto start = Clock::now();
	auto testDuration = 5.f;
	while (true) {
		auto now = Clock::now();
		// elapsed time in seconds as a float
		std::chrono::duration<float> elapsed = now - start;
		auto t = elapsed.count() / testDuration;        // normalized 0->1 over 5 seconds
		t = std::clamp(t, 0.0f, 1.0f);					 // ensure it never exceeds [0,1]

		AudioManager_SetFloatValue("player", "velocity", t);

		if (elapsed.count() >= testDuration)
			break;				// exit after 5 seconds
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	AudioManager_Destroy();
	std::cout << "END" << std::endl;
}

static void RunSelectNodeTest() {
	LogMessage("=== Select Node Test ===", LogCategory::CLI, LogLevel::Info);

	auto cfg = GetTestConfig();
	AudioManager_Create(&cfg);
	AudioManager_LoadBehaviorsFromFile(behaviorPath, assetPath);
	AudioManager_SetTag("l", "listener");
	AudioManager_SetVectorValue("l", "position", 0, 0, 0);
	AudioManager_SetTag("player", "entity.tester");
	AudioManager_SetVectorValue("player", "position", 5, 0, 0);
	AudioManager_SetTag("player", "test.select");


	auto start = Clock::now();
	auto testDuration = 5.f;
	while (true) {
		auto now = Clock::now();
		// elapsed time in seconds as a float
		std::chrono::duration<float> elapsed = now - start;
		int val = int(elapsed.count()) % 2;

		AudioManager_SetStringValue("player", "velocity", std::to_string(val).c_str());

		if (elapsed.count() >= testDuration)
			break;				// exit after 5 seconds
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	AudioManager_Destroy();
	std::cout << "END" << std::endl;
}

static void RunParallelNodeTest() {
	LogMessage("=== Parallel Node Test ===", LogCategory::CLI, LogLevel::Info);

	auto cfg = GetTestConfig();
	AudioManager_Create(&cfg);
	AudioManager_LoadBehaviorsFromFile(behaviorPath, assetPath);
	AudioManager_SetTag("l", "listener");
	AudioManager_SetVectorValue("l", "position", 0, 0, 0);
	AudioManager_SetTag("player", "entity.tester");
	AudioManager_SetVectorValue("player", "position", 5, 0, 0);
	AudioManager_SetTag("player", "test.parallel");


	auto start = Clock::now();
	auto testDuration = 5.f;

	while (true) {
		auto now = Clock::now();
		// elapsed time in seconds as a float
		std::chrono::duration<float> elapsed = now - start;
		int val = int(elapsed.count()) % 2;



		if (elapsed.count() >= testDuration)
			break;				// exit after 5 seconds
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	AudioManager_Destroy();
	std::cout << "END" << std::endl;
}
static void RunSequenceNodeTest() {
	LogMessage("=== Sequence Node Test ===", LogCategory::CLI, LogLevel::Info);

	auto cfg = GetTestConfig();
	AudioManager_Create(&cfg);
	AudioManager_LoadBehaviorsFromFile(behaviorPath, assetPath);
	AudioManager_SetTag("l", "listener");
	AudioManager_SetVectorValue("l", "position", 0, 0, 0);
	AudioManager_SetTag("player", "entity.tester");
	AudioManager_SetVectorValue("player", "position", 5, 0, 0);
	AudioManager_SetTag("player", "test.sequence");


	auto start = Clock::now();
	auto testDuration = 5.f;

	while (true) {
		auto now = Clock::now();
		// elapsed time in seconds as a float
		std::chrono::duration<float> elapsed = now - start;
		int val = int(elapsed.count()) % 2;



		if (elapsed.count() >= testDuration)
			break;				// exit after 5 seconds
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	AudioManager_Destroy();
	std::cout << "END" << std::endl;
}


static void RunRandomNodeTest() {
	LogMessage("=== Random Node Test ===", LogCategory::CLI, LogLevel::Info);

	auto cfg = GetTestConfig();
	AudioManager_Create(&cfg);
	AudioManager_LoadBehaviorsFromFile(behaviorPath, assetPath);
	AudioManager_SetTag("l", "listener");
	AudioManager_SetVectorValue("l", "position", 0, 2, 2);
	AudioManager_SetTag("player", "entity.tester");
	AudioManager_SetVectorValue("player", "position", 2, 0, 0);
	AudioManager_SetTag("player", "test.random");


	auto start = Clock::now();
	auto testDuration = 5.f;
	int count = 0;
	while (true) {
		auto now = Clock::now();
		// elapsed time in seconds as a float
		std::chrono::duration<float> elapsed = now - start;
		int val = int(elapsed.count()) % 2;

		count++;
		if (count % 10 == 0) {
			AudioManager_SetTransientTag("player", "foot.contact.left");
		}



		if (elapsed.count() >= testDuration)
			break;				// exit after 5 seconds
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	AudioManager_Destroy();
	std::cout << "END" << std::endl;
}












std::queue<std::function<void()>> gCommands;
std::mutex                  gCmdMutex;

static void EnqueueCommand(std::function<void()> cmd) {
	std::lock_guard<std::mutex> lg(gCmdMutex);
	gCommands.push(std::move(cmd));
}

static void ProcessCommands() {
	std::lock_guard<std::mutex> lg(gCmdMutex);
	while (!gCommands.empty()) {
		gCommands.front()();
		gCommands.pop();
	}
}

static void RunInteractiveTest() {



	LogMessage("=== Random Node Test ===", LogCategory::CLI, LogLevel::Info);

	auto cfg = GetTestConfig();
	AudioManager_Create(&cfg);
	AudioManager_LoadBehaviorsFromFile(behaviorPath, assetPath);
	AudioManager_SetTag("l", "listener");
	AudioManager_SetVectorValue("l", "position", 0, 0, 0);
	AudioManager_SetTag("player", "entity.tester");
	AudioManager_SetVectorValue("player", "position", 5, 0, 0);
	AudioManager_SetTag("player", "test.random");

	std::atomic<bool> running{ true };


	// Input thread: blocks on getline, parses lines, enqueues actions
	std::thread inputThread([&]() {
		std::string line;
		while (running && std::getline(std::cin, line)) {


			std::istringstream iss(line);
			std::string cmd; iss >> cmd;
			if (cmd == "exit") {
				running = false;
				AudioManager_Destroy();
				//break;
			}
			else if (cmd == "tag") {
				std::string entity, tag;
				iss >> entity >> tag;
				EnqueueCommand([=]() {
					AudioManager_SetTag(entity.c_str(), tag.c_str());
					});
			}
			else if (cmd == "clear") {
				std::string entity, tag;
				iss >> entity >> tag;
				EnqueueCommand([=]() {
					AudioManager_ClearTag(entity.c_str(), tag.c_str());
					});
			}
			else if (cmd == "transient") {
				std::string entity, tag;
				iss >> entity >> tag;
				EnqueueCommand([=]() {
					AudioManager_SetTransientTag(entity.c_str(), tag.c_str());
					});
			}
			else if (cmd == "float") {
				std::string entity, key; float v;
				iss >> entity >> key >> v;
				EnqueueCommand([=]() {
					AudioManager_SetFloatValue(entity.c_str(), key.c_str(), v);
					});
			}
			else if (cmd == "string") {
				std::string entity, key; std::string v;
				iss >> entity >> key >> v;
				EnqueueCommand([=]() {
					AudioManager_SetStringValue(entity.c_str(), key.c_str(), v.c_str());
					});
			}
			else if (cmd == "pos") {
				std::string entity; float x, y, z;
				iss >> entity >> x >> y >> z;
				EnqueueCommand([=]() {
					AudioManager_SetVectorValue(entity.c_str(), "position", x, y, z);
					});

			}
			else if (cmd == "log") {
				EnqueueCommand([=]() {
					AudioManager_DebugPrintState();
					});
			}
			else {
				std::cout << "Unknown command. Use:\n"
					"  tag <entity> <tag>\n"
					"  clear <entity> <tag>\n"
					"  transient <entity> <tag>\n"
					"  float <entity> <key> <float>\n"
					"  string <entity> <key> <string>\n"
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
			ProcessCommands();
			last = now;
		}
		// sleep a bit to avoid busy-spin
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	inputThread.join();
	AudioManager_Destroy();
}





int main() {

	//RunInteractiveTest(); // <- this. is. so. cool! xD

	RunRandomNodeTest();
	//RunBlendNodeTest();
	//RunSelectNodeTest();

	 //RunSequenceNodeTest();
	 //RunParallelNodeTest();


	return 0;
}


