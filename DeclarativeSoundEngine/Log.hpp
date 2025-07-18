#pragma once
#include "declsound_export.hpp"
#include <string>
#include <string_view>
#include <source_location>
#include <iostream>
#include <mutex>

using LogCallbackFn = void(*)(int category, int level, const char* message);


enum class LogCategory {

	CLI,
	General,
	AudioCore,
	AudioDevice,
	AudioBuffer,
	AudioManager,
	BehaviorLoader,
	Entity,
	Leaf,
	Parser,
};

enum class LogLevel {
	Trace,
	Debug,
	Info,
	Warning,
	Error
};

// TODO: Move all exposed api functions to AudioManagerAPI!

extern "C" DECLSOUND_API void AudioManager_LogSetMinimumLevel(LogCategory category, LogLevel level);
extern "C" DECLSOUND_API void LogMessageC(const char* message, int category, int level);

inline void LogMessage(const std::string& message, LogCategory category, LogLevel level) {
	LogMessageC(message.c_str(), static_cast<int>(category), static_cast<int>(level));
}

void LogFunctionCall(const std::source_location& loc = std::source_location::current());

extern "C"  DECLSOUND_API void AudioManager_SetLogCallback(LogCallbackFn cb);

extern "C" DECLSOUND_API bool AudioManager_PollLog(int* outCat, int* outLvl, char* outMsg, int maxLen);
