// audioManager.hpp
#pragma once
#include <unordered_map>
#include "AudioCommand.hpp"
#include "AudioDevice.hpp"
#include "SoundManagerAPI.hpp"



struct Bus;
struct Voice;
class Entity;
class AudioBufferManager;
struct AudioConfig;


class AudioManager
{

	std::unordered_map<std::string, Entity> entities{};
	std::vector<BehaviorDef> definitions{};

	AudioBufferManager* bufferManager;
	std::unique_ptr<AudioDevice> device;
	uint64_t		globalSampleCounter = 0;
	AudioConfig*		deviceCfg;

	CommandQueue*		inQueue;
	CommandQueue*		outQueue;

	std::mutex			snapshotMutex;
	std::vector<Voice>	voiceSnapShots;
	std::string			currentListener = {};

	std::vector<Bus>                buses;           // [0]=master
	std::unordered_map<std::string, int> entityBus;

	bool shouldQuit = false;
public:
	AudioManager(AudioConfig* deviceCfg, CommandQueue* inQueue, CommandQueue* outQueue);
	void ThreadMain();
	~AudioManager();

	void Update(float dt);

	void Shutdown();


	void TakeSnapshot();

	void SetTag(Command cmd);
	void SetTransient(Command cmd);
	void ClearTag(Command cmd);
	void SetValue(Command cmd);
	void ClearValue(Command cmd);

	void LoadBehaviorsFromFolder(Command cmd);

private:

	inline static std::atomic<int>	gFront{ 0 };                // index the callback sees
	inline static int				gBuild = 1;                 // control fills here
	std::atomic<uint32_t>			pendingFrames{ 0 };

	void RenderCallback(float* output, int nFrames);
};

