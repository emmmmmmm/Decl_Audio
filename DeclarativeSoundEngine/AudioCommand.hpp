#pragma once
#include <string>
#include <queue>
#include "AudioBehavior.hpp"
#include <memory>
#include "RingBuffer.hpp"

enum class CommandType {
	StartBehavior,
	StopBehavior,
	ValueUpdate,
	Log,
	PlaySuccess,
	RefreshDefinitions,
};

struct Command {
	CommandType type;
	std::string entityId, key, strValue, soundName;
	float       value;
	uint32_t instanceID;
	uint32_t    behaviorId;

	Command() = default;
	Command(const Command&) = default;          // allow copying
	Command& operator=(const Command&) = default;
	Command(Command&&) = default;
	Command& operator=(Command&&) = default;
	
};


static constexpr size_t CMD_QUEUE_SIZE = 64;
using CommandQueue = RingBuffer<Command, CMD_QUEUE_SIZE>;