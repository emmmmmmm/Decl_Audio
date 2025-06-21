// audioManager.hpp
#pragma once
#include <unordered_map>
#include "AudioCommand.hpp"
#include "AudioDevice.hpp"
#include "Entity.hpp"
#include "SpeakerLayout.hpp"


struct Bus;
struct Voice;
class Entity;
class AudioBufferManager;
struct AudioConfig;

struct Tag {
	std::string entity;
	std::string tag;
	bool transient;
};

class AudioManager
{
	std::unordered_map<std::string, Entity> entities{};
	std::vector<BehaviorDef>				definitions{};

	std::vector<Tag>		newTags;
	std::vector<Tag>	removedTags;
	std::unordered_map<std::string, std::vector<BehaviorDef*>> exactMatchMap;
	std::vector<std::pair<std::string, BehaviorDef*>> wildcardMatchers;

	AudioBufferManager*						bufferManager;
	std::unique_ptr<AudioDevice>			device;
	uint64_t								globalSampleCounter = 0;
	AudioConfig*							deviceCfg;

	CommandQueue*							inQueue;
	CommandQueue*							outQueue;

	std::mutex								snapshotMutex;
	std::vector<Voice>						voiceSnapShots;
	std::string								currentListener = {};

	std::vector<Bus>						buses; // [0]=master
	std::unordered_map<std::string, int>	entityBus;

	std::atomic<bool>						shouldQuit{ false };
private:

	std::atomic<int>						currentReadBuffer = 0;
	std::atomic<uint32_t>					cbBlockIndex{ 0 };

	std::atomic<uint32_t>					pendingFrames{ 0 };

	SpeakerLayout speakerLayout;

public:
	AudioManager(AudioConfig* deviceCfg, CommandQueue* inQueue, CommandQueue* outQueue);

	void ThreadMain();
	~AudioManager();

	void Update(float dt);

	void Shutdown();

	void TakeSnapshot();

	void AdvancePlayheads();

	int GetOrCreateBus(const std::string& entityId);

	void SetTag(Command cmd);
	void SetTransient(Command cmd);
	void ClearTag(Command cmd);
	void SetValue(Command cmd);
	void ClearValue(Command cmd);
	void SetAssetPath(Command cmd);
	void ClearEntity(Command cmd);
	void LoadBehaviorsFromFolder(Command cmd);

	void DebugPrintState();

private:
	void RenderCallback(float* output, int nFrames);
	void RenderCallbackOld(float* out, int frames);
};

