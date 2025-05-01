#include "pch.h"
#include "SoundManager.hpp"
#include "Log.hpp"
#include <sstream>
#include <regex>
#include "AudioDevice.hpp"
#include <string>

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
}

void SoundManager::SetTag(const std::string& entityId, const std::string& tag) {
	entityTags[entityId].AddTag(tag);
	LogMessage("tag added: " + tag + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);
}

void SoundManager::ClearTag(const std::string& entityId, const std::string& tag) {
	LogMessage("tag removed: " + tag + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);

	entityTags[entityId].RemoveTag(tag);
}

void SoundManager::SetValue(const std::string& entityId, const std::string& key, float value) {
	entityValues[entityId].SetValue(key, value);
	LogMessage("set value: " + std::to_string(value) + " key: " + key+ "(entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);

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
			LogMessage("removed behavior", LogCategory::SoundManager, LogLevel::Debug);
		}

	active = desired;   // snapshot for next frame
}



void SoundManager::Update()
{
	const TagMap& globalTags = entityTags["global"];

	for (auto& [entityId, tags] : entityTags) {
		// --- pick best match  ---
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

