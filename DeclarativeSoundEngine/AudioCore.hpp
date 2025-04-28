//AudioCore.hpp
#pragma once
#include "AudioCommand.hpp"
#include "AudioBehavior.hpp"
#include "AudioBufferManager.hpp"
#include "AudioDevice.hpp"
#include "IBehaviorDefinition.hpp"
class SoundManager; // forward declaration


struct Voice {
	SoundHandle   handle;
	float         currentVolume;
	float         targetVolume;
	float         currentPitch;
	float         targetPitch;
	size_t        busIndex;
	// ... ramp steps, position, instanceID etc.
};

struct BehaviorInstance {
	uint32_t             instanceID;
	std::vector<Voice>   voices;

	std::unique_ptr<Node> rootNode;   // own the clone
	// ... node graph, parameter map, dirty flag
};

struct EntityData {
	std::unordered_map<std::string, float> parameters;   // e.g. position, velocity…
	std::vector<BehaviorInstance>   instances;          // all alive behaviors for this entity

};
class AudioCore {
public:
	AudioCore(IBehaviorDefinition* defsProvider, CommandQueue* fromManager, CommandQueue* toManager);
	~AudioCore();
	void Update();
	void ProcessCommands();
	void HandleStartBehavior(const Command& cmd);
	void HandleStopBehavior(const Command& cmd);
	void HandleValueUpdate(const Command& cmd);

	AudioBufferManager* audioBufferManager;
	std::unique_ptr<AudioDevice>   device;
	std::unordered_map<std::string, EntityData> entityMap;
	std::atomic<uint32_t>          nextInstanceID{ 1 };

private:
	// Internal sound data and behavior management
	IBehaviorDefinition* defsProvider;
	std::vector<AudioBehavior> activeBehaviors;
	CommandQueue* inQueue;
	CommandQueue* outQueue;

	void ProcessActiveSounds();

	void RefreshDefinitions();
	std::unordered_map<uint32_t, Node*> behaviorRegistry;
	SoundNode* FindFirstSoundNode(Node* node);
};

