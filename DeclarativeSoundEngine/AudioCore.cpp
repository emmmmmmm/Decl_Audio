// AudioCore.cpp
#include "pch.h"
#include "AudioCore.hpp"
#include "Log.hpp"
#include "AudioDevice.hpp"
#include "AudioDeviceStub.hpp"
#include "IBehaviorDefinition.hpp"
#include <iostream>

AudioCore::AudioCore(IBehaviorDefinition* defsProvider, CommandQueue* fromManager, CommandQueue* toManager)
	: defsProvider(defsProvider), inQueue(fromManager), outQueue(toManager)
{
	audioBufferManager = new AudioBufferManager();
	device = std::make_unique<AudioDeviceStub>();

	// DEBUG:
	/*
	Command testCommand;
	testCommand.type = CommandType::Log;
	testCommand.strValue = "DebugTestLogThingyFromCore";
	outQueue->push(testCommand);
	*/

}
AudioCore::~AudioCore() {}

void AudioCore::Update() {
	// this function will loop indefinitely in the future! 

	ProcessCommands();
	ProcessActiveSounds();
}

void AudioCore::ProcessCommands() {
	LogMessage("ProcessCommands", LogCategory::AudioCore, LogLevel::Debug);
	Command cmd;
	while (inQueue->pop(cmd)) {
		LogMessage(cmd.entityId, LogCategory::AudioCore, LogLevel::Debug);
		switch (cmd.type) {
		case CommandType::StartBehavior:
			HandleStartBehavior(cmd);
			break;
		case CommandType::StopBehavior:
			HandleStopBehavior(cmd);
			break;
		case CommandType::ValueUpdate:
			HandleValueUpdate(cmd);
			break;
		case CommandType::RefreshDefinitions:
			RefreshDefinitions();
			break;
		default:
			break;
		}
	}
}

void AudioCore::RefreshDefinitions() {
	LogMessage("::RefreshDefinitions", LogCategory::AudioCore, LogLevel::Debug);
	for (auto& p : defsProvider->GetPlayDefs()) {
		behaviorRegistry[p.id] = p.rootNode.get();
	}
	LogMessage("Audiocore definitions refreshed!", LogCategory::AudioCore, LogLevel::Debug);
}

void AudioCore::HandleStartBehavior(const Command& cmd) {
	LogMessage("AudioCore: StartBehavior called for " + cmd.soundName, LogCategory::AudioCore, LogLevel::Info);
	// 1) Allocate a new instance
	uint32_t iid = nextInstanceID++;
	auto& entity = entityMap[cmd.entityId];                  // auto-create if missing

	entity.instances.emplace_back();
	BehaviorInstance& inst = entity.instances.back();
	inst.instanceID = iid;
	auto it = behaviorRegistry.find(cmd.behaviorId);
	if (it == behaviorRegistry.end()) {
		LogMessage("Unknown behavior ID " + std::to_string(cmd.behaviorId),
			LogCategory::AudioCore, LogLevel::Warning);
		return;
	}
	auto proto = it->second;           // guaranteed non-null
	inst.rootNode = proto->clone();       // deep‐copy for this instance

	// 2) For MVP: assume the behavior has a single SoundNode leaf
	//    and that Command.behavior points to an AudioBehavior you loaded earlier.

	Node* root = inst.rootNode.get();
	auto* leaf = FindFirstSoundNode(root);
	if (!leaf) {
		// no playable leaf—emit an error back to SoundManager
		LogMessage("No playable leaf found in graph!", LogCategory::AudioCore, LogLevel::Warning);
		return;
	}
	std::string soundFile = leaf->sound;    // Assuming SoundNode has a .soundName memberfloat        vol = 1; //behavior.EvaluateVolume(cmd);   // stubbed as 1.0f for now
	float pitch = 1;                        //inst.EvaluatePitch(cmd);    // stubbed as 1.0f
	float vol = 1;


	// 3) Load or reuse the buffer
	AudioBuffer* buf;
	auto bufferloaded = audioBufferManager->TryLoad(soundFile, buf);
	if (!bufferloaded) {
		Command w;
		w.type = CommandType::Log;
		w.soundName = soundFile;
		w.strValue = "failed to load " + soundFile;
		outQueue->push(w);

		return; // nothing to play here.
	}

	
	// 4) Kick off playback
	SoundHandle handle = device->Play(buf, vol, pitch, /*loop=*/false);

	// 5) Record the new voice
	Voice v;
	v.handle = handle;
	v.currentVolume = vol;
	v.targetVolume = vol;
	v.currentPitch = pitch;
	v.targetPitch = pitch;

	//v.busIndex = entityBusMap[cmd.entityId];           // lookup or allocate bus
	inst.voices.push_back(v);

	// 6) (Optional) emit a success event
	Command evt;
	evt.type = CommandType::PlaySuccess;
	evt.entityId = cmd.entityId;
	evt.instanceID = iid;
	outQueue->push(evt);
	LogMessage("End of HandleStartBehavior()", LogCategory::AudioCore, LogLevel::Debug);

}

void AudioCore::HandleStopBehavior(const Command& cmd) {
	LogMessage("AudioCore: StopBehavior called for " + cmd.soundName, LogCategory::AudioCore, LogLevel::Info);
	// todo: stop sound, cleanup from activebehaviors
}


void AudioCore::HandleValueUpdate(const Command& cmd) {
	LogMessage("AudioCore: ValueUpdate " + cmd.key + " = " + std::to_string(cmd.value), LogCategory::AudioCore, LogLevel::Info);
	// TODO: actually update values for behaviors
}

void AudioCore::ProcessActiveSounds() {
	// In the future, this will handle sound playback, fading, etc.
	// sounds that have ended will need to clean themselfs up from activeBehaviors as well

}


// Helper: find the first SoundNode in a node-tree
SoundNode* AudioCore::FindFirstSoundNode(Node* node) {
	if (auto* sn = dynamic_cast<SoundNode*>(node)) {
		return sn;
	}
	for (auto& childPtr : node->children) {
		if (auto* found = FindFirstSoundNode(childPtr.get())) {
			return found;
		}
	}
	return nullptr;
}
