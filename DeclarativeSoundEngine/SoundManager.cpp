#include "pch.h"
#include "SoundManager.hpp"
#include "Log.hpp"
#include <sstream>
#include <regex>
#include "AudioDevice.hpp"
#include <string>
#include "SoundManagerAPI.hpp"


#include "AudioDeviceMiniAudio.hpp"
#include "AudioDeviceUnity.hpp"

SoundManager::SoundManager(AudioConfig* cfg)
{
	defsProvider = new BehaviorDefinitionManager();

	std::unique_ptr<AudioDevice> dev;    // pointer to the base
	if (cfg->backend == Miniaudio)
		dev = std::make_unique<AudioDeviceMiniaudio>(cfg->channels, cfg->sampleRate, cfg->bufferFrames);

	if (cfg->backend == Unity)
		dev = std::make_unique<AudioDeviceUnity>(cfg->channels, cfg->sampleRate, cfg->bufferFrames);



	audioCore = new AudioCore(defsProvider, &managerToCore, &coreToManager, std::move(dev), cfg);
	// todo: detach audio core as separate thread
}



SoundManager::~SoundManager()
{
	LogMessage("~SoundManager", LogCategory::SoundManager, LogLevel::Debug);
	delete audioCore;	// that wont work if i don't keep a ref to audiocore later... 
	// it'd kind of have to clean itself up?^^

	delete defsProvider;

}


void SoundManager::AddBehavior(AudioBehavior& behavior) {
	LogMessage("::Addbehavior is obsolete!!", LogCategory::SoundManager, LogLevel::Warning);
}

void SoundManager::SetTag(const std::string& entityId, const std::string& tag) {
	entityTags[entityId].AddTag(tag);
	LogMessage("tag added: " + tag + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);
}

void SoundManager::ClearTag(const std::string& entityId, const std::string& tag) {
	LogMessage("tag removed: " + tag + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);

	entityTags[entityId].RemoveTag(tag);
}

void SoundManager::SetTransientTag(const std::string& entityId, const std::string& tag)
{
	entityTags[entityId].AddTag(tag, true);
	LogMessage("Transient Tag added: " + tag + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);
}

void SoundManager::SetValue(const std::string& entityId, const std::string& key, float value) {
	entityValues[entityId].SetValue(key, value);
	LogMessage("set value: " + std::to_string(value) + " key: " + key + "(entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);

}

void SoundManager::ClearValue(const std::string& entityId, const std::string& key) {
	entityValues[entityId].ClearValue(key);
	LogMessage("clear Value: " + key + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);

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

void SoundManager::SetAssetPath(const std::string& path)
{
	assetpath = path;
	// we need to pass this along to audiocore if it changes!
	// aaactually: audiocore doesn't really care I think!
	// but maybe it does.
	Command c;
	c.type = CommandType::AssetPath;
	c.strValue = assetpath;
	managerToCore.push(c);
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

// UNUSUED?
void SoundManager::SyncBehaviors(const std::string& entityId, const std::unordered_set<uint32_t>& desired)
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



			// TODO - this no longer works as expected. ideally we would get a 
			// "Behavior active" and "Behavior stopped" message from audiocore, 
			// to make sure behaviors have actually started.
			lastEmittedSoundIds.push_back(std::to_string(id));
		}


	// stop obsolete
	for (uint32_t id : active)
		if (!desired.contains(id)) {
			Command c;
			c.type = CommandType::StopBehavior;
			c.entityId = entityId;
			c.behaviorId = id;
			managerToCore.push(c);
			LogMessage("removed behavior", LogCategory::SoundManager, LogLevel::Debug);
		}

	active = desired;   // snapshot for next frame
}




void SoundManager::SyncBehaviorsForEntity(const std::string& entityId, const TagMap& tags, const TagMap& globalTags)
{
	auto& data = activeBehaviors[entityId];

	// 1) Gather all matching definitions
	std::unordered_set<uint32_t> desired;
	for (const auto& md : matchDefinitions) {
		if (MatchScore(md, tags, globalTags, entityId) >= 0) {
			desired.insert(md.id);
		}
	}

	// 2) Start newly desired behaviors
	for (auto id : desired) {
		if (!data.count(id)) {

			Command c;
			c.type = CommandType::StartBehavior;
			c.entityId = entityId;
			c.behaviorId = id;
			managerToCore.push(c);
			data.insert(id);
		}
	}

	// 3) Stop behaviors no longer desired
	std::vector<uint32_t> toStop;
	for (auto id : data) {
		if (!desired.count(id)) {
			toStop.push_back(id);
		}
	}
	for (auto id : toStop) {
		Command c;
		c.type = CommandType::StopBehavior;
		c.entityId = entityId;
		c.behaviorId = id;
		managerToCore.push(c);
		data.erase(id);
	}
}


void SoundManager::Update()
{


	const TagMap& globalTags = entityTags["global"];

	for (auto& [entityId, tags] : entityTags) {
		SyncBehaviorsForEntity(entityId, tags, globalTags);
		SendValueDiff(entityId);
		tags.ClearTransient();
	}

	audioCore->Update(); // TODO: remove once audiocore gets detached into it's own thread!

	ProcessCoreResponses(); // for later I guess
}

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

