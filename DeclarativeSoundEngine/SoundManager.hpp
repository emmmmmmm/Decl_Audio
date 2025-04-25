#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "AudioBehavior.hpp"
#include "TagMap.hpp"
#include "ValueMap.hpp"

class SoundManager {
public:
	void Update();

	void AddBehavior(const AudioBehavior& behavior);
	void SetTag(const std::string& entityId, const std::string& tag);
	void ClearTag(const std::string& entityId, const std::string& tag);
	void SetValue(const std::string& entityId, const std::string& key, float value);
	void DebugPrintState();

private:
	std::vector<AudioBehavior> behaviors;
	std::unordered_map<std::string, TagMap> entityTags;
	std::unordered_map<std::string, ValueMap> entityValues;
};