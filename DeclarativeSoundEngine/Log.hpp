#pragma once
#include "declsound_export.hpp"
#include <string>
#include <string_view>
#include <source_location>
#include <iostream>

enum class LogCategory {
    SoundManager,
    BehaviorLoader, BehaviorDefMgr,
    CLI,
    General, 
    AudioCore, 
    AudioDevice,
    AudioBuffer,
};

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error
};

DECLSOUND_API void LogSetMinimumLevel(LogCategory category, LogLevel level);
extern "C" DECLSOUND_API void LogMessageC(const char* message, int category, int level);

inline void LogMessage(const std::string& message, LogCategory category, LogLevel level) {
    LogMessageC(message.c_str(), static_cast<int>(category), static_cast<int>(level));
}
 void LogFunctionCall(
    const std::source_location& loc = std::source_location::current()
);
