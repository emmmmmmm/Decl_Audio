#pragma once
#include <string>
#include <queue>
#include "AudioBehavior.hpp"
#include <memory>

enum class CommandType {
	StartBehavior,
	StopBehavior,
	ValueUpdate,
	Log
};

struct Command {
	CommandType type;
	std::string entityId, key, strValue, soundName;
	float       value;
	std::shared_ptr<AudioBehavior> behavior;  // has a unique_ptr inside

	Command() = default;
	Command(const Command&) = default;          // allow copying
	Command& operator=(const Command&) = default;
	Command(Command&&) = default;
	Command& operator=(Command&&) = default;
};

using CommandQueue = std::queue<Command>;