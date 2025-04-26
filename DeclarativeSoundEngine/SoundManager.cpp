#include "pch.h"
#include "SoundManager.hpp"
#include "Log.hpp"
#include <sstream>
#include <regex>


SoundManager::SoundManager()
{
	audioCore = new AudioCore(this, &managerToCore, &coreToManager);
	// todo: detach audio core as separate thread

}

SoundManager::~SoundManager()
{
	delete audioCore;
}

void SoundManager::AddBehavior(AudioBehavior& behavior) {
	behaviors.emplace_back(behavior);
	LogMessage("Behavior added: " + behavior.id, LogCategory::SoundManager, LogLevel::Trace);
}

void SoundManager::SetTag(const std::string& entityId, const std::string& tag) {
	entityTags[entityId].AddTag(tag);
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


int SoundManager::MatchScore(const AudioBehavior& behavior, const TagMap& entityMap, const TagMap& globalMap, const std::string& entityId) {
	int score = 0;
	auto allTags = entityMap.GetAllTags();
	const auto& globalTags = globalMap.GetAllTags();
	allTags.insert(allTags.end(), globalTags.begin(), globalTags.end());

	for (const auto& required : behavior.matchTags) {
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

	for (const auto& condition : behavior.matchConditions) {
		if (!condition.eval(entityVals, globalVals)) {
			return -1;
		}
	}

	return score;
}



void SoundManager::Update() {
	lastEmittedSoundIds.clear();
	const TagMap& globalMap = entityTags["global"];

	for (const auto& [entityId, tagMap] : entityTags) {
		int bestScore = -1;
		const AudioBehavior* best = nullptr;

		for (const auto& behavior : behaviors) {
			int score = MatchScore(behavior, tagMap, globalMap, entityId);
			if (score > bestScore) {
				bestScore = score;
				best = &behavior;
			}
		}

		if (best) {
			//LogMessage("Entity " + entityId + " matched behavior: " + best->id + " -> PlaySound: " + best->soundName, LogCategory::SoundManager, LogLevel::Info);

			Command cmd;
			cmd.type = CommandType::StartBehavior;
			cmd.entityId = entityId;
			cmd.behavior = std::make_shared<AudioBehavior>(*best);
			/*
				TODO:
				cmd.behavior = std::shared_ptr<AudioBehavior>(behaviors_shared_ptrs[i]);
				(if you refactor behaviors into vector<shared_ptr<AudioBehavior>>).
			*/
			cmd.soundName = best->id;
			managerToCore.push(cmd);


			lastEmittedSoundIds.push_back(cmd.soundName);
		}
	}

	audioCore->Update(); // TODO: remove once audiocore gets detached into it's own thread!

	ProcessCoreResponses();
}



void SoundManager::ProcessCoreResponses() {
	// lots, callbacks, etc
	while (!coreToManager.empty()) {
		const Command cmd = coreToManager.front();
		coreToManager.pop();

		// Handle the response - logging, etc.
		if (cmd.type == CommandType::Log) {
			LogMessage("CORE: " + cmd.strValue, LogCategory::AudioCore, LogLevel::Info);
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
