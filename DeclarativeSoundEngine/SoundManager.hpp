#pragma once
#include <vector>
#include "AudioBehavior.hpp"
#include "TagMap.hpp"
#include "ValueMap.hpp"

class SoundManager {
public:
    void Update();

    void AddBehavior(const AudioBehavior& behavior);
    void SetTag(const std::string& tag);
    void ClearTag(const std::string& tag);
    void SetValue(const std::string& key, float value);

private:
    std::vector<AudioBehavior> behaviors;
    TagMap tagMap;
    ValueMap valueMap;
};