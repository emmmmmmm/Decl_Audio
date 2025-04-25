#include "pch.h"
#include "BehaviorLoader.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>

std::vector<AudioBehavior> LoadAudioBehaviorsFromFile(const std::string& path) {
	std::vector<AudioBehavior> out;

	YAML::Node root = YAML::LoadFile(path);
	for (const auto& node : root) {
		AudioBehavior behavior;
		behavior.id = node["id"].as<std::string>();
		behavior.soundName = node["soundName"].as<std::string>();

		if (node["matchTags"]) {
			for (const auto& tagNode : node["matchTags"]) {
				behavior.matchTags.push_back(tagNode.as<std::string>());
			}
		}

		out.push_back(behavior);
	}

	return out;
}
