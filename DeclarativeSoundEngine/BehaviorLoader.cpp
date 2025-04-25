#include "pch.h"
#include "BehaviorLoader.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include "Log.hpp"

std::vector<AudioBehavior> LoadAudioBehaviorsFromFile(const std::string& path) {
	std::vector<AudioBehavior> out;

     LogMessage("Opening YAML file: " + path, LogCategory::BehaviorLoader, LogLevel::Trace);
    YAML::Node root = YAML::LoadFile(path);
    LogMessage("YAML loaded.", LogCategory::BehaviorLoader, LogLevel::Trace);

    for (const auto& node : root) {
        LogMessage("Parsing behavior node...", LogCategory::BehaviorLoader, LogLevel::Trace);
        AudioBehavior behavior;
        behavior.id = node["id"].as<std::string>();
        behavior.soundName = node["soundName"].as<std::string>();

        if (node["matchTags"]) {
            for (const auto& tagNode : node["matchTags"]) {
                behavior.matchTags.push_back(tagNode.as<std::string>());
            }
        }

        if (node["matchConditions"]) {
            for (const auto& condNode : node["matchConditions"]) {
                behavior.matchConditions.push_back(condNode.as<std::string>());
            }
        }

        LogMessage("Behavior parsed: " + behavior.id, LogCategory::BehaviorLoader, LogLevel::Trace);
        out.push_back(behavior);
    }

    LogMessage("Finished loading all behaviors.", LogCategory::BehaviorLoader, LogLevel::Debug);
    return out;
}
