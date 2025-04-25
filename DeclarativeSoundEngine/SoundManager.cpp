#include "pch.h"
#include "SoundManager.hpp"
#include <iostream>

void SoundManager::AddBehavior(const AudioBehavior& behavior) {
	behaviors.push_back(behavior);
}

void SoundManager::SetTag(const std::string& entityId, const std::string& tag) {
	entityTags[entityId].AddTag(tag);
}

void SoundManager::ClearTag(const std::string& entityId, const std::string& tag) {
	entityTags[entityId].RemoveTag(tag);
}

void SoundManager::SetValue(const std::string& entityId, const std::string& key, float value) {
	entityValues[entityId].SetValue(key, value);
}

void SoundManager::Update() {
	for (const auto& [entityId, tagMap] : entityTags) {
		for (const auto& behavior : behaviors) {
			bool matched = true;
			for (const auto& tag : behavior.matchTags) {
				if (!tagMap.HasTag(tag) && !entityTags["global"].HasTag(tag)) {
					matched = false;
					break;
				}
			}
			if (matched) {
				std::cout << "[SoundManager] Entity " << entityId << " matched behavior: "
					<< behavior.id << " -> Emitting PlaySound for " << behavior.soundName << std::endl;
			}
		}
	}
}

void SoundManager::DebugPrintState() {
	for (const auto& [entityId, tagMap] : entityTags) {
		std::cout << "[SoundManager] Entity: " << entityId << "\nTags:" << std::endl;
		for (const auto& tag : tagMap.GetAllTags()) {
			std::cout << " - " << tag << std::endl;
		}

		std::cout << "Values:" << std::endl;
		for (const auto& pair : entityValues[entityId].GetAllValues()) {
			std::cout << " - " << pair.first << " = " << pair.second << std::endl;
		}
	}
}