//AudioCore.hpp


#pragma once
#include "AudioCommand.hpp"
#include "BehaviorDef.hpp"
#include "AudioBufferManager.hpp"
#include "AudioDevice.hpp"
#include "IBehaviorDefinition.hpp"
#include <algorithm>
#include "ValueMap.hpp"
#include <atomic>         
#include "ObjectFactory.hpp"
#include "SoundManagerAPI.hpp"
#include "Vec3.hpp"
#include "LeafBuilder.hpp"

class SoundManager; // forward declaration

// TODO: maxvoices and maxbusses should be set in audiocore ctr! (and passed from soundmanager via Init cmd)

constexpr int  kMaxVoices = 256;                 // worst-case active voices
constexpr int  kMaxBuses = 16;                  // master + sub-buses
constexpr int  kSnapCount = 3;                   // triple-buffer
//static size_t   kBufSamples = 0;      // set in AudioCore() ctor



struct Voice {
	SoundHandle handle = {};
	const AudioBuffer* buffer = {};
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

	// concept from ObjectFactory
	void reset() {
		// e.g. reset playhead, clear buffers, zero flags…
		playhead = {};
		buffer = {};
		// etc.
	}
};



struct BehaviorInstance {

	enum class Phase { Start, Active, Ending };

	uint32_t                    id = {};
	Phase                       phase{ Phase::Start };
	AudioConfig*				deviceCfg;
	std::unique_ptr<Node>		onStart;
	std::unique_ptr<Node>		onActive;
	std::unique_ptr<Node>		onEnd;

	std::vector<Voice>          voices;

	std::unordered_map<std::string, Expression> paramExpr;
	
	
	//AudioBufferManager* bufferManager;



	//static void CollectLeaves(const Node&, const ValueMap&, float, uint8_t,
	//	std::vector<SoundLeaf>&, AudioBufferManager*, bool loop = false);


	bool HasVoice(const SoundNode* src) const
	{
		return std::any_of(voices.begin(), voices.end(),
			[&](const Voice& v) { return v.source == src; });
	};

	void StartVoice(const LeafBuilder::Leaf& leaf, int busIdx, uint64_t currentSample, const ValueMap& params)
	{

		std::cout << "start leaf, loop: " + std::to_string(leaf.loop) <<" offset: " <<std::to_string(leaf.startSample)<< std::endl;
		Voice v;
		v.buffer = leaf.buffer;
		v.playhead = 0;
		v.currentVol = v.targetVol = leaf.volume(params);
		//v.targetPitch = leaf.pitch(params); // TBD
		v.loop = leaf.loop;
		v.busIndex = busIdx;
		v.source = leaf.src;
		v.startSample = leaf.startSample;
		voices.push_back(std::move(v));
	};

	void Update(const ValueMap& params, AudioBufferManager* bufMgr, uint8_t busIdx, float dt, uint64_t nowSamples);

	void reset() {
		// e.g. reset playhead, clear buffers, zero flags…
		id = {};
		phase = Phase::Start;
		voices = {};
		paramExpr = {};
		onStart = {};
		onActive = {};
		onEnd = {};


		// etc.
	}
};

struct VoiceSnap {
	const AudioBuffer*	buf;
	size_t				playhead;
	float				gain;
	bool				loop;
	uint8_t				bus;

	uint64_t startSample = {};
	// Spatialization
	std::vector<float> pan ={};
};



/* -------- immutable snapshot -------- */
struct Snapshot {
	Vec3		  listenerPosition = {};
	uint32_t      voiceCount = 0;
	VoiceSnap     voices[kMaxVoices];

	uint32_t      busCount = 1;                  // at least master
	float         busGain[kMaxBuses]{};
	std::vector<int> busParent{};

	mutable std::vector<float> bus[kMaxBuses];  // resised once in ctor
};


struct EntityData {
	ValueMap params;
	std::vector<BehaviorInstance*> instances;
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
	
	std::vector<Voice> voiceBuffer;
	std::mutex voiceBufferMtx;

	// mix‐graph
	std::vector<Bus>                buses;           // [0]=master
	std::unordered_map<std::string, int> entityBus;

	// runtime state
	std::unordered_map<std::string, EntityData> entityMap;
	std::unordered_map<uint32_t, PlayDefinition> prototypes;
	//AudioBufferManager* bufferMgr = {};

	std::vector<Voice> voiceSnapShots;

	uint32_t nextInstanceID = {};

	IBehaviorDefinition* defsProvider;
	CommandQueue* inQueue;
	CommandQueue* outQueue;

	std::mutex snapshotMutex;


	/* ----------- engine-wide storage ----------- */
	inline static std::atomic<int>gFront{ 0 };                // index the callback sees
	inline static int             gBuild = 1;                 // control fills here

	std::atomic<uint32_t>  pendingFrames{ 0 };

	std::chrono::steady_clock::time_point lastUpdate;

	uint64_t globalSampleCounter = 0;
	AudioConfig* deviceCfg;

	std::string currentListener = {};
public:
	AudioBufferManager* audioBufferManager;
	std::unique_ptr<AudioDevice> device;


	ObjectFactory<Voice> voiceFactory{ 200, 50 }; // TBD.
	ObjectFactory<BehaviorInstance> behaviorFactory{ 100, 5 };

public:
	AudioCore(
		IBehaviorDefinition* defsProvider, 
		CommandQueue* fromManager,
		CommandQueue* toManager, 
		std::unique_ptr<AudioDevice> audioDevice,
		AudioConfig* deviceCfg);

	~AudioCore();

	void Update();
	void AdvancePlayheads();
private:

	// methods
	void TakeSnapshot();
	void ProcessCommands();
	void DebugDumpEntityMap();
	void RefreshDefinitions();
	void HandleStartBehavior(const Command& cmd);
	PlayDefinition* GetPrototype(uint32_t behaviorId);
	std::string GetBehaviorName(uint32_t behaviorId);
	void HandleStopBehavior(const Command& cmd);
	void HandleValueUpdate(const Command& cmd);
	void SetAssetPath(const Command& cmd);
	void ProcessActiveSounds(float dt);
	SoundNode* FindFirstSoundNode(Node* node);
	void RenderCallback(float* output, int nFrames);
	int  GetOrCreateBus(const std::string&);
	void ClearBusBuffers();
	void HandleBusGain(const Command& cmd);

	AudioConfig* GetDeviceConfig() { return deviceCfg; };

	/*Utility*/
	static inline bool VoiceFinished(const Voice& v)
	{
		if (!v.buffer)return true; //!??
		return !v.loop && v.playhead >= v.buffer->GetFrameCount();
	}

	static inline bool AllFinished(const std::vector<Voice>& vv)
	{
		return std::all_of(vv.begin(), vv.end(), VoiceFinished);
	}
};
