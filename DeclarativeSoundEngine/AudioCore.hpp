//AudioCore.hpp
#pragma once
#include "AudioCommand.hpp"
#include "AudioBehavior.hpp"
#include "AudioBufferManager.hpp"
#include "AudioDevice.hpp"
#include "IBehaviorDefinition.hpp"
#include <algorithm>
#include "ValueMap.hpp"
#include <atomic>         



class SoundManager; // forward declaration

/* -------- constants you already know -------- */
constexpr int  kMaxVoices = 256;                 // worst-case active voices
constexpr int  kMaxBuses = 16;                  // master + sub-buses
constexpr int  kSnapCount = 3;                   // triple-buffer
//static size_t   kBufSamples = 0;      // set in AudioCore() ctor

struct Vec3 { float x, y, z; };

struct Voice {
	SoundHandle handle = {};
	AudioBuffer* buffer = {};
	size_t       playhead = {};      // sample-frame read offset
	float        currentVol = {};
	float        targetVol = {};
	float        volStep = {};
	int          busIndex = {};
	uint64_t startSample = {};


	bool loop = false;
	const SoundNode* source = nullptr;


	// optional: pitch fields when you add resampling
	float currentPitch = {};
	float targetPitch = {};



	Voice MakeSnapShot() const {
		return *this;
	}
};

struct SoundLeaf {
	const SoundNode* src;     // unique identity in graph
	AudioBuffer* buf;
	float				gain;
	bool				loop;
	uint8_t				bus;
};




enum class Phase { Start, Active, Ending };

struct BehaviorInstance {
	uint32_t                    id = {};
	Phase                       phase{ Phase::Start };
	std::unique_ptr<Node> onStart;
	std::unique_ptr<Node> onActive;
	std::unique_ptr<Node> onEnd;

	std::vector<Voice>          voices;

	std::unordered_map<std::string, Expression> paramExpr;

	
	

	static void CollectLeaves(const Node&, const ValueMap&, float, uint8_t,
		std::vector<SoundLeaf>&, AudioBufferManager*);


	bool HasVoice(const SoundNode* src) const
	{
		return std::any_of(voices.begin(), voices.end(),
			[&](const Voice& v) { return v.source == src; });
	};

	void StartVoice(const SoundLeaf& leaf, int busIdx , uint64_t currentSample)
	{
		Voice v;
		v.buffer = leaf.buf;
		v.playhead = 0;
		v.currentVol = v.targetVol = leaf.gain;
		v.loop = leaf.loop;
		v.busIndex = busIdx;
		v.source = leaf.src;
		v.startSample = currentSample;
		voices.push_back(std::move(v));
	};

	void Update(const ValueMap& params, AudioBufferManager* bufMgr, uint8_t busIdx, float dt, uint64_t nowSamples);

};

struct VoiceSnap {
	AudioBuffer* buf;
	size_t       playhead;
	float        gain;
	bool         loop;
	uint8_t      bus;

	uint64_t startSample;

};



/* -------- immutable snapshot -------- */
struct Snapshot {
	uint32_t      voiceCount = 0;
	VoiceSnap     voices[kMaxVoices];

	uint32_t      busCount = 1;                  // at least master
	float         busGain[kMaxBuses];

	mutable std::vector<float> bus[kMaxBuses];  // resised once in ctor
};


struct EntityData {
	ValueMap                 params;
	std::vector<BehaviorInstance>         instances;
};

struct Bus {
	std::vector<float> buffer;
	Vec3               position = {};
	Expression		   volume{ "1.0" };

	struct Routing { int dst; float gain; };

	std::vector<Routing> sends;     // extra fan-outs
	int parent = 0;      // 0 = master, or another sub-mix
};


class AudioCore {
	// config
	int bufferFrames = 1024;
	int outputChannels = 2;
	int sampleRate = 44100;
	int bitDepth = 16;
	std::vector<Voice> voiceBuffer;
	std::mutex voiceBufferMtx;

	// mix‐graph
	std::vector<Bus>                buses;           // [0]=master
	std::unordered_map<std::string, int> entityBus;

	// runtime state
	std::unordered_map<std::string, EntityData> entityMap;
	std::unordered_map<uint32_t, PlayDefinition> prototypes;
	AudioBufferManager* bufferMgr = {};

	std::vector<Voice> voiceSnapShots;

	uint32_t nextInstanceID = {};

	IBehaviorDefinition* defsProvider;
	CommandQueue* inQueue;
	CommandQueue* outQueue;

	std::mutex snapshotMutex;


	/* ----------- engine-wide storage ----------- */
	//static Snapshot        gSnapshots[kSnapCount];				// never freed
	inline static std::atomic<int>gFront{ 0 };                 // index the callback sees
	inline static int             gBuild = 1;                 // control fills here

	std::atomic<uint32_t>  pendingFrames{ 0 };

	std::chrono::steady_clock::time_point lastUpdate;

	 uint64_t globalSampleCounter = 0;

public:
	AudioBufferManager* audioBufferManager;
	std::unique_ptr<AudioDevice> device;
	uint64_t GetGlobalSampleCounter() { return globalSampleCounter; }

public:
	AudioCore(IBehaviorDefinition* defsProvider, CommandQueue* fromManager, CommandQueue* toManager);

	~AudioCore();

	void Update();
	void AdvancePlayheads();
private:
	// methods
	void TakeSnapshot();
	void ProcessCommands();
	void RefreshDefinitions();
	void HandleStartBehavior(const Command& cmd);
	void HandleStopBehavior(const Command& cmd);
	void HandleValueUpdate(const Command& cmd);
	void ProcessActiveSounds(float dt);
	SoundNode* FindFirstSoundNode(Node* node);
	void RenderCallback(float* output, int nFrames);
	int  GetOrCreateBus(const std::string&);
	void ClearBusBuffers();
	void HandleBusGain(const Command& cmd);

	/*Utility*/
	static inline bool VoiceFinished(const Voice& v)
	{
		return !v.loop && v.playhead >= v.buffer->GetFrameCount();
	}

	static inline bool AllFinished(const std::vector<Voice>& vv)
	{
		return std::all_of(vv.begin(), vv.end(), VoiceFinished);
	}
};
