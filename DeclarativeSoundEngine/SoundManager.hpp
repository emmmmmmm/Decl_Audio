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

class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    void Update();

    void AddBehavior(AudioBehavior& behavior);
    void SetTag(const std::string& entityId, const std::string& tag);
    void ClearTag(const std::string& entityId, const std::string& tag);
    void SetValue(const std::string& entityId, const std::string& key, float value);
    void ClearValue(const std::string& entityId, const std::string& key);
    void ClearEntity(const std::string& entityId);
    void SetBusGain(const std::string& entityId, float gain);             // literal
    void SetBusGainExpr(const std::string& entityId, const std::string& gain); // expression
    void SetAssetPath(const std::string& path);

    void DebugPrintState();


    std::vector<std::string> lastEmittedSoundIds;   // for tests. in the future we might want to wait
                                                    // for the "started audio" message from audiocore?

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
    void SyncBehaviors(const std::string& entityId, const std::unordered_set<uint32_t>& desired);

    int MatchScore(const MatchDefinition& behavior, const TagMap& entityMap, const TagMap& globalMap, const std::string& entityId);
    int TagSpecificity(const std::string& tag);
    bool TagMatches(const std::string& pattern, const std::string& actual);
   
    AudioCore* audioCore; // AudioCore instance
    void ProcessCoreResponses(); // Process responses from AudioCore
    
};
