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

void SoundManager::SetTag(const std::string& entityId, const std::string& tag) {
	entityTags[entityId].AddTag(tag);
	LogMessage("tag added: " + tag + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);


	// LISTENER // TODO is there a better place to put this? :thinking:
	if (tag == "listener") {
		Command c;
		c.type = CommandType::SetListener;
		c.entityId = entityId;
		managerToCore.push(c);
	}
}

void SoundManager::ClearTag(const std::string& entityId, const std::string& tag) {
	LogMessage("tag removed: " + tag + " (entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);

	entityTags[entityId].RemoveTag(tag);

	// LISTENER // TODO is there a better place to put this? :thinking:
	if (tag == "listener") {
		Command c;
		c.type = CommandType::RemoveListener;
		c.entityId = entityId;
		managerToCore.push(c);
	}
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

void SoundManager::SetValue(const std::string& entityId, const std::string& key, const std::string& value)
{
	entityValues[entityId].SetValue(key, value);
	LogMessage("set value: " + value + " key: " + key + "(entity: " + entityId + ")", LogCategory::SoundManager, LogLevel::Debug);
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
	c.value = assetpath;
	managerToCore.push(c);
}

void SoundManager::SetEntityPosition(const std::string& entityId, float x, float y, float z)
{
	entityValues[entityId].SetValue("position", Vec3(x, y, z));

}

void SoundManager::SendValueDiff(const std::string& entityId)
{
	const ValueMap& current = entityValues[entityId];
	ValueMap& last = lastValues[entityId];           // creates empty on first use

	for (auto& [k, v] : current.GetAllValues()) {
		if (!last.HasValue(k) || last.values[k] != v) {
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


static std::string JoinTags(const std::vector<std::string>& tags) {
	std::ostringstream oss;
	oss << "[";
	for (size_t i = 0; i < tags.size(); ++i) {
		oss << tags[i];
		if (i + 1 < tags.size()) oss << ", ";
	}
	oss << "]";
	return oss.str();
}

void SoundManager::SyncBehaviorsForEntity(const std::string& entityId, const TagMap& tags, const TagMap& globalTags)
{
	auto& data = activeBehaviors[entityId];

	// 1) Gather all matching definitions
	std::unordered_set<uint32_t> desired;
	for (const auto& md : matchDefinitions) {
		auto score = MatchUtils::MatchScore(md, tags, globalTags, entityValues, entityId);


		/*std::cerr
			<< "[Debug]   candidate " << md.name
			<< " → score=" << score
			<< " id: " << md.id
			<< "  entitytags=" << JoinTags(tags.GetAllTags())
			<< "  matchtags=" << JoinTags(md.matchTags)
			<< "\n";*/

		if (score >= 0) {

			desired.insert(md.id);

		}
	}

	// 2) Start newly desired behaviors
	for (auto id : desired) {
		if (!data.count(id)) {
			std::cerr << "starting new id: " << id << std::endl;

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
	for (auto& [entityId, tags] : entityTags) {
		SyncBehaviorsForEntity(entityId, tags, entityTags["global"]);
		SendValueDiff(entityId);
		tags.ClearTransient();
	}
	audioCore->Update(); // TODO: remove once audiocore gets detached into it's own thread!
}





void SoundManager::DebugPrintState() {
	for (const auto& [entityId, tagMap] : entityTags) {
		LogMessage("Entity: " + entityId, LogCategory::SoundManager, LogLevel::Debug);

		for (const auto& tag : tagMap.GetAllTags()) {
			LogMessage(" - Tag: " + tag, LogCategory::SoundManager, LogLevel::Debug);
		}

		for (const auto& pair : entityValues[entityId].GetAllValues()) {
			std::string value = "";
			if (auto s = std::get_if<std::string>(&pair.second))
				value = *s;
			else if (auto s = std::get_if<float>(&pair.second))
				value = std::to_string(*s);
			else if (auto s = std::get_if<Vec3>(&pair.second))
				value = "( " + std::to_string(s->x) + " / " + std::to_string(s->y) + " / " + std::to_string(s->z) + " )";
			LogMessage(" - Value: " + pair.first + " = " + value, LogCategory::SoundManager, LogLevel::Debug);
		}
	}
}

