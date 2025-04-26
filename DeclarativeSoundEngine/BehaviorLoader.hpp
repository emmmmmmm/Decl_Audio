#pragma once
#include "AudioBehavior.hpp"
#include <vector>
#include <string>
#include "ASTNode.hpp"

class BehaviorLoader {
public:
    // Scan folder for .audio (or .yaml) files and build raw behaviors
    static std::vector<AudioBehavior> LoadAudioBehaviorsFromFolder(const std::string& folderPath);

};