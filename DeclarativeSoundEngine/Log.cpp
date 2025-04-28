#include "pch.h"
#include "Log.hpp"
#include <iostream>
#include <unordered_map>

static std::unordered_map<LogCategory, LogLevel> minimumLevels = {
	{ LogCategory::SoundManager, LogLevel::Debug },
	{ LogCategory::BehaviorLoader, LogLevel::Debug },
	{ LogCategory::CLI, LogLevel::Debug },
	{ LogCategory::General, LogLevel::Debug }
};

static const char* ToString(LogLevel level) {
	switch (level) {
	case LogLevel::Trace: return "Trace";
	case LogLevel::Debug: return "Debug";
	case LogLevel::Info: return "Info";
	case LogLevel::Warning: return "Warning";
	case LogLevel::Error: return "Error";
	default: return "Unknown";
	}
}

static const char* ToString(LogCategory category) {
	switch (category) {
	case LogCategory::SoundManager: return "SoundManager";
	case LogCategory::BehaviorLoader: return "BehaviorLoader";
	case LogCategory::CLI: return "CLI";
	case LogCategory::General: return "General";
	case LogCategory::AudioCore: return "AudioCore";
	default: return "Unknown";
	}
}

void LogSetMinimumLevel(LogCategory category, LogLevel level) {
	minimumLevels[category] = level;
}

void LogMessageC(const char* message, int category, int level) {
	LogCategory cat = static_cast<LogCategory>(category);
	LogLevel lvl = static_cast<LogLevel>(level);
	if (lvl >= minimumLevels[cat]) {
		std::cout << "[Log-" << ToString(cat) << "] " << message << std::endl;
	}
}

void LogFunctionCall(const std::source_location& loc)
{
	/*
	How it works:
		loc.function_name() returns
		"void __cdecl SoundManager::Update(void)".
		fn.find('(') locates the parameter‐list start.
		fn.rfind(' ', par) finds the space just before your class-scope name.
		fn.remove_prefix(sp+1) chops off "void __cdecl " (or any return type + calling convention).
	*/

	std::string_view fn = loc.function_name();
	if (auto par = fn.find('('); par != fn.npos) {
		if (auto sp = fn.rfind(' ', par); sp != fn.npos)
			fn.remove_prefix(sp + 1);
	}

	std::cout
		<< fn //loc.function_name()
		<< " ("
		// << loc.file_name()
		<< "line:" << loc.line()
		<< ")"
		<< std::endl;
}
