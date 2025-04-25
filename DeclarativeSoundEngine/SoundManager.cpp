#include "pch.h"
#include "SoundManager.hpp"
#include <iostream>
#include "stdio.h"

void SoundManager::AddBehavior(const AudioBehavior& behavior) {
    behaviors.push_back(behavior);
}

void SoundManager::SetTag(const std::string& tag) {
    tagMap.AddTag(tag);
}

void SoundManager::ClearTag(const std::string& tag) {
    tagMap.RemoveTag(tag);
}

void SoundManager::SetValue(const std::string& key, float value) {
    valueMap.SetValue(key, value);
}

void SoundManager::Update() {
    for (const auto& behavior : behaviors) {
        bool matched = true;
        for (const auto& tag : behavior.matchTags) {
            if (!tagMap.HasTag(tag)) {
                matched = false;
                break;
            }
        }

        if (matched) {
            std::cout << "[SoundManager] Matched behavior: " << behavior.id
                << " -> Emitting PlaySound for " << behavior.soundName << std::endl;
        }
    }


}

void SoundManager::DebugPrintState() {
    std::cout << "[SoundManager] Current Tags:" << std::endl;
    for (const auto& tag : tagMap.GetAllTags()) {
        std::cout << " - " << tag << std::endl;
    }

    std::cout << "[SoundManager] Current Values:" << std::endl;
    for (const auto& pair : valueMap.GetAllValues()) {
        std::cout << " - " << pair.first << " = " << pair.second << std::endl;
    }
}