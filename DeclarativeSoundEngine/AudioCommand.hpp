#pragma once
#include <string>
#include <queue>
#include <memory>
#include "BehaviorDef.hpp"
#include "RingBuffer.hpp"

enum class CommandType {
	None,
	Log,
	AssetPath,
	SetTag,
	SetTransient,
	ClearTag,
	SetValue,
	ClearValue,
	ClearEntity,
	LoadBehaviors,
	Shutdown,
};

struct Command {
	CommandType type = CommandType::None;
	std::string entityId = {};
	std::string key = {};
	std::string soundName = {};
	Value		value; 
	uint32_t	instanceID = -1;
	uint32_t	behaviorId = -1;

	Command() = default;
	Command(const Command&) = default;  // allow copying
	Command& operator=(const Command&) = default;
	Command(Command&&) = default;
	Command& operator=(Command&&) = default;

	std::string GetTypeName() {
		switch (type) {
		case CommandType::AssetPath: return "AssetPath";
		case CommandType::Log: return "Log";
		case CommandType::SetTag: return "SetTag";
		case CommandType::ClearTag: return "ClearTag";
		case CommandType::SetTransient: return "SetTransient";
		case CommandType::SetValue: return "SetValue";
		case CommandType::ClearValue:return "ClearValue";
		case CommandType::LoadBehaviors:return "LoadBehaviors";
		case CommandType::Shutdown:return "Shutdown";
		case CommandType::ClearEntity:return "ClearEntity";
		default: return "Unknown";
		}
	}
};


static constexpr size_t CMD_QUEUE_SIZE = 1024;
using CommandQueue = RingBuffer<Command, CMD_QUEUE_SIZE>;