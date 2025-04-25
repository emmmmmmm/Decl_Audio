#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "AudioBehavior.hpp"
#include "TagMap.hpp"
#include "ValueMap.hpp"
#include "AudioCommand.hpp"
#include "AudioCore.hpp"

class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    void Update();

    void AddBehavior(const AudioBehavior behavior);
    void SetTag(const std::string& entityId, const std::string& tag);
    void ClearTag(const std::string& entityId, const std::string& tag);
    void SetValue(const std::string& entityId, const std::string& key, float value);
    void ClearValue(const std::string& entityId, const std::string& key);
    void ClearEntity(const std::string& entityId);
    void DebugPrintState();

    std::vector<std::string> lastEmittedSoundIds;  // for tests

    CommandQueue managerToCore;
    CommandQueue coreToManager;
private:
    std::vector<AudioBehavior> behaviors;
    std::unordered_map<std::string, TagMap> entityTags;
    std::unordered_map<std::string, ValueMap> entityValues;

    int MatchScore(const AudioBehavior& behavior, const TagMap& entityMap, const TagMap& globalMap, const std::string& entityId);
    int TagSpecificity(const std::string& tag);
    bool TagMatches(const std::string& pattern, const std::string& actual);
    bool EvaluateCondition(const std::string& condition, const ValueMap& entityVals, const ValueMap& globalVals);

    AudioCore* audioCore; // AudioCore instance
    void ProcessCoreResponses(); // Process responses from AudioCore
    
};
