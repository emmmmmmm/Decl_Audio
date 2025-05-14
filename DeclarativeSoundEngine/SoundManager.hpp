#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "AudioBehavior.hpp"
#include "TagMap.hpp"
#include "ValueMap.hpp"
#include "AudioCommand.hpp"
#include "AudioCore.hpp"
#include "BehaviorDefinitionManager.hpp"

#include "SoundManagerAPI.hpp"
#include "MatchUtils.hpp"



class SoundManager {
public:
	SoundManager(AudioConfig* cfg);
	~SoundManager();

	void Update();

	void SetTag(const std::string& entityId, const std::string& tag);
	void ClearTag(const std::string& entityId, const std::string& tag);
	void SetTransientTag(const std::string& entityId, const std::string& tag);
	void SetValue(const std::string& entityId, const std::string& key, float value);
	void SetValue(const std::string& entityId, const std::string& key, const std::string& value);
	void ClearValue(const std::string& entityId, const std::string& key);
	void ClearEntity(const std::string& entityId);
	void SetBusGain(const std::string& entityId, float gain);             // literal
	void SetBusGainExpr(const std::string& entityId, const std::string& gain); // expression
	void SetAssetPath(const std::string& path);
	void SetEntityPosition(const std::string& entityName, float x, float y, float z);
	void DebugPrintState();


	BehaviorDefinitionManager* defsProvider;

	CommandQueue managerToCore;
	CommandQueue coreToManager;

	std::vector<MatchDefinition> matchDefinitions;
private:
	std::string assetpath{};
	std::unordered_map<std::string, TagMap> entityTags;
	std::unordered_map<std::string, ValueMap> entityValues;

	std::unordered_map<std::string, std::unordered_set<uint32_t>> activeBehaviors; // entity -> playing IDs
	std::unordered_map<std::string, ValueMap>                     lastValues;      // entity -> values pushed to core
	void SendValueDiff(const std::string& entityId);

	void SyncBehaviorsForEntity(const std::string& entityId, const TagMap& tags, const TagMap& globalTags);

	
	AudioCore* audioCore; // AudioCore instance

};
