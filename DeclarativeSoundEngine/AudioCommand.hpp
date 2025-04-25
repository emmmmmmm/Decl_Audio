#pragma once
#include <string>
#include <queue>
#include "AudioBehavior.hpp"

enum class CommandType {
    StartBehavior,
    StopBehavior,
    ValueUpdate,
    Log
};

struct Command {
    CommandType type;
    // Entity, behavior data, or value to update
    std::string entityId;
    std::string key;
    float value; // For value updates
    std::string strValue; // for logs etc
    std::string soundName; // For behaviors
    AudioBehavior behavior;
};

using CommandQueue = std::queue<Command>;