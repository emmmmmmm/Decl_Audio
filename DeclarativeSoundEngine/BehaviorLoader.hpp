#pragma once
#include "AudioBehavior.hpp"
#include <vector>
#include <string>
#include "ASTNode.hpp"
#include "Condition.hpp"
#include "Expression.hpp"



// Blueprint for audio behaviors parsed from YAML
struct BehaviorDef {
    std::string id;
    int busIndex = 0;
    std::vector<std::string> matchTags;
    std::vector<Condition> matchConditions;
    Expression rootVolume{ "1.0" };
    std::unique_ptr<Node> onStart;
    std::unique_ptr<Node> onActive;
    std::unique_ptr<Node> onEnd;
};


class BehaviorLoader {
public:
    // Scan folder for .audio (or .yaml) files and build raw behaviors
    static std::vector<AudioBehavior> LoadAudioBehaviorsFromFolder(const std::string& folderPath);

};