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
