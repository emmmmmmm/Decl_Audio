#include "pch.h"
#include "SoundManager.hpp"
#include "Log.hpp"
#include <sstream>
#include <regex>
#include "AudioDevice.hpp"

SoundManager::SoundManager() : defsProvider(new BehaviorDefinitionManager())   
{
	audioCore = new AudioCore(defsProvider, &managerToCore, &coreToManager);
	// todo: detach audio core as separate thread

}

SoundManager::~SoundManager()
{
	delete audioCore;	// that wont work if i don't keep a ref to audiocore later... 
						// it'd kind of have to clean itself up?^^

	delete defsProvider;

}

void SoundManager::AddBehavior(AudioBehavior& behavior) {
	LogMessage("::Addbehavior is obsolete!!", LogCategory::SoundManager, LogLevel::Warning);
	//behaviors.emplace_back(behavior);
	//LogMessage("Behavior added: " + behavior.name, LogCategory::SoundManager, LogLevel::Trace);
}

void SoundManager::SetTag(const std::string& entityId, const std::string& tag) {
	entityTags[entityId].AddTag(tag);
	LogMessage("tag added: " + tag + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);
}

void SoundManager::ClearTag(const std::string& entityId, const std::string& tag) {
	entityTags[entityId].RemoveTag(tag);
}

void SoundManager::SetValue(const std::string& entityId, const std::string& key, float value) {
	entityValues[entityId].SetValue(key, value);
}

void SoundManager::ClearValue(const std::string& entityId, const std::string& key) {
	entityValues[entityId].ClearValue(key);
}

void SoundManager::ClearEntity(const std::string& entityId) {
	entityTags.erase(entityId);
	entityValues.erase(entityId);
	LogMessage("Entity cleared: " + entityId, LogCategory::SoundManager, LogLevel::Debug);
}

void SoundManager::SetBusGain(const std::string& entityId, float gain)
{
	LogMessage("SetBusGain: " + entityId + " gain: " + std::to_string(gain), LogCategory::SoundManager, LogLevel::Debug);
	LogMessage("SetBusGain: NOT IMPLEMENTED", LogCategory::SoundManager, LogLevel::Warning);
}

void SoundManager::SetBusGainExpr(const std::string& entityId, const std::string& gain)
{
	LogMessage("SetBusGainExpr: " + entityId + " gain: " + gain, LogCategory::SoundManager, LogLevel::Debug);
	LogMessage("SetBusGainExpr: NOT IMPLEMENTED", LogCategory::SoundManager, LogLevel::Warning);

}

int SoundManager::TagSpecificity(const std::string& tag) {
	std::istringstream stream(tag);
	std::string segment;
	int score = 0;

	while (std::getline(stream, segment, '.')) {
		score += (segment == "*") ? 5 : 10;
	}

	return score;
}

bool SoundManager::TagMatches(const std::string& pattern, const std::string& actual) {
	std::istringstream pat(pattern);
	std::istringstream act(actual);
	std::string pseg, aseg;

	while (std::getline(pat, pseg, '.')) {
		if (!std::getline(act, aseg, '.')) return false;
		if (pseg != "*" && pseg != aseg) return false;
	}

	return !std::getline(act, aseg, '.');
}


int SoundManager::MatchScore(const MatchDefinition& md, const TagMap& entityMap, const TagMap& globalMap, const std::string& entityId) {
	int score = 0;
	auto allTags = entityMap.GetAllTags();
	const auto& globalTags = globalMap.GetAllTags();
	allTags.insert(allTags.end(), globalTags.begin(), globalTags.end());

	for (const auto& required : md.matchTags) {
		bool matched = false;
		for (const auto& actual : allTags) {
			if (TagMatches(required, actual)) {
				matched = true;
				score += 10 + TagSpecificity(required);
				break;
			}
		}
		if (!matched) return -1;
	}

	static const ValueMap emptyVals;
	const ValueMap& entityVals = entityValues.count(entityId) ? entityValues.at(entityId) : emptyVals;
	const ValueMap& globalVals = entityValues.count("global") ? entityValues.at("global") : emptyVals;

	for (const auto& condition : md.matchConditions) {
		if (!condition.eval(entityVals, globalVals)) {
			return -1;
		}
	}

	return score;
}

void SoundManager::SendValueDiff(const std::string& entityId)
{
	const ValueMap& current = entityValues[entityId];
	ValueMap& last = lastValues[entityId];           // creates empty on first use

	for (auto [k, v] : current.GetAllValues()) {
		if (!last.HasValue(k) || last.GetValue(k) != v) {
			Command c;
			c.type = CommandType::ValueUpdate;
			c.entityId = entityId;
			c.key = k;
			c.value = v;
			managerToCore.push(c);
			last.SetValue(k, v);
		}
	}
}
void SoundManager::SyncBehaviors(const std::string& entityId,
	const std::unordered_set<uint32_t>& desired)
{
	auto& active = activeBehaviors[entityId];

	// start new
	for (uint32_t id : desired)
		if (!active.contains(id)) {
			Command c;
			c.type = CommandType::StartBehavior;
			c.entityId = entityId;
			c.behaviorId = id;
			managerToCore.push(c);
		}

	// stop obsolete
	for (uint32_t id : active)
		if (!desired.contains(id)) {
			Command c;
			c.type = CommandType::StopBehavior;
			c.entityId = entityId;
			c.behaviorId = id;
			managerToCore.push(c);
		}

	active = desired;   // snapshot for next frame
}



void SoundManager::Update()
{
	const TagMap& globalTags = entityTags["global"];

	for (auto& [entityId, tags] : entityTags) {
		// --- pick best match (keep algorithm you already have) ---
		int bestScore = -1;
		const MatchDefinition* best = nullptr;

		for (const auto& md : matchDefinitions) {
			int score = MatchScore(md, tags, globalTags, entityId);
			if (score > bestScore) { bestScore = score; best = &md; }
		}

		std::unordered_set<uint32_t> desired;
		if (best) desired.insert(best->id);     // for now single-match

		SyncBehaviors(entityId, desired);
		SendValueDiff(entityId);
	}

	audioCore->Update(); // TODO: remove once audiocore gets detached into it's own thread!

	ProcessCoreResponses();
}


/*
void SoundManager::Update() {
	LogFunctionCall();
	lastEmittedSoundIds.clear();
	const TagMap& globalMap = entityTags["global"];

	for (const auto& [entityId, tagMap] : entityTags) {
		int bestScore = -1;
		const MatchDefinition* best = nullptr;

		for (const auto& md : matchDefinitions) {
			int score = MatchScore(md, tagMap, globalMap, entityId);
			if (score > bestScore) {
				bestScore = score;
				best = &md;
			}
		}

		if (best) {

			// TODO: Soundmanager needs to keep track of spawned behaviors, 
			// otherwise it will spam spawn events every Update as long as the 
			// triggercondition is true!


			//LogMessage("Entity " + entityId + " matched behavior: " + best->id + " -> PlaySound: " + best->soundName, LogCategory::SoundManager, LogLevel::Info);

			Command cmd;
			cmd.type = CommandType::StartBehavior;
			cmd.entityId = entityId;
			cmd.soundName = best->name;
			cmd.behaviorId = best->id;
			managerToCore.push(cmd);
			LogMessage("Update: Added Command to Queue: "+best->name, LogCategory::SoundManager, LogLevel::Info);


			lastEmittedSoundIds.push_back(cmd.soundName);
		}
	}
	audioCore->Update(); // TODO: remove once audiocore gets detached into it's own thread!
	
	ProcessCoreResponses();
}
*/


void SoundManager::ProcessCoreResponses() {
	// logs, callbacks, etc
	Command cmd;
	while (coreToManager.pop(cmd)) {
		// Handle the response - logging, etc.
		if (cmd.type == CommandType::Log) {
			LogMessage("CORE Response: " + cmd.strValue, LogCategory::AudioCore, LogLevel::Info);
		}

	}
}



void SoundManager::DebugPrintState() {
	for (const auto& [entityId, tagMap] : entityTags) {
		LogMessage("Entity: " + entityId, LogCategory::SoundManager, LogLevel::Debug);

		for (const auto& tag : tagMap.GetAllTags()) {
			LogMessage(" - Tag: " + tag, LogCategory::SoundManager, LogLevel::Debug);
		}

		for (const auto& pair : entityValues[entityId].GetAllValues()) {
			LogMessage(" - Value: " + pair.first + " = " + std::to_string(pair.second), LogCategory::SoundManager, LogLevel::Debug);
		}
	}
}

void SoundManager::BufferTest()
{

	LogMessage("SoundManager::BufferTest()", LogCategory::CLI, LogLevel::Info);


	return;
	// repeat load of same file:
	AudioBuffer* buf1;
	AudioBuffer* buf2;
	bool handle1 = audioCore->audioBufferManager->TryLoad("test.wav", buf1); 
	bool handle2 = audioCore->audioBufferManager->TryLoad("test.wav", buf2); 
	LogMessage(handle1 && handle2 && buf1 == buf2  ? "SUCCESS" : "FAILED", LogCategory::CLI, LogLevel::Info);

	// file not found:
	AudioBuffer* buf3;
	bool handle3 = audioCore->audioBufferManager->TryLoad("unknown.wav", buf3); 
	
	// call to audio device
	SoundHandle h = audioCore->device->Play(buf1, 1.0f, 1.0f, false);
	LogMessage(h > 0 ? "STUB PLAY OK" : "STUB PLAY FAIL", LogCategory::CLI, LogLevel::Info);



}
