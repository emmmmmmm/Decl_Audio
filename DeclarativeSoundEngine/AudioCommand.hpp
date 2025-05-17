#pragma once
#include <string>
#include <queue>
#include <memory>
#include "BehaviorDef.hpp"
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
	SetListener,
	RemoveListener,
	None,
	// NEW:
	SetTag,
	SetTransient,
	ClearTag,
	SetValue,
	ClearValue,
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
		case CommandType::StartBehavior: return "StartBehavior";
		case CommandType::StopBehavior: return "StopBehavior";
		case CommandType::ValueUpdate: return "ValueUpdate";
		case CommandType::BusGainUpdate: return "BusGainUpdate";
		case CommandType::RefreshDefinitions: return "RefreshDefinitions";
		case CommandType::AssetPath: return "AssetPath";
		case CommandType::Log: return "Log";
		case CommandType::PlaySuccess: return "PlaySuccess";
		case CommandType::SetListener: return "SetListener";
		case CommandType::RemoveListener: return "RemoveListener";
		case CommandType::SetTag: return "SetTag";
		case CommandType::ClearTag: return "ClearTag";
		case CommandType::SetTransient: return "SetTransient";
		case CommandType::SetValue: return "SetValue";
		case CommandType::ClearValue:return "ClearValue";
		case CommandType::LoadBehaviors:return "LoadBehaviors";
		case CommandType::Shutdown:return "Shutdown";
		default: return "Unknown";
		}
	}
};


static constexpr size_t CMD_QUEUE_SIZE = 64;
using CommandQueue = RingBuffer<Command, CMD_QUEUE_SIZE>;