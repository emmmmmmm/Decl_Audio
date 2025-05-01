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
	BusGainUpdate,
	Log,
	PlaySuccess,
	RefreshDefinitions,
	AssetPath,
	None
};

struct Command {
	CommandType type = CommandType::None;
	std::string entityId = {};
	std::string key = {};
	std::string strValue = {};
	std::string soundName = {};
	float       value = 0;
	uint32_t instanceID = -1;
	uint32_t    behaviorId = -1;

	Command() = default;
	Command(const Command&) = default;          // allow copying
	Command& operator=(const Command&) = default;
	Command(Command&&) = default;
	Command& operator=(Command&&) = default;

	std::string GetTypeName() {
		switch (type) {
		case CommandType::StartBehavior: return "StartBehavior";
		case CommandType::StopBehavior: return "StopBehavior";
		case CommandType::ValueUpdate: return "ValueUpdate";
		case CommandType::BusGainUpdate: return "BusGainUpdate";
		case CommandType::RefreshDefinitions: return "RefreshDefinitions";
		case CommandType::AssetPath: return "AssetPath";
		case CommandType::Log: return "Log";
		default: return "Unknown";
		}
	}

};


static constexpr size_t CMD_QUEUE_SIZE = 64;
using CommandQueue = RingBuffer<Command, CMD_QUEUE_SIZE>;