#pragma once
#include "BehaviorDef.hpp"
#include <vector>
#include <string>
#include "Condition.hpp"
#include "Expression.hpp"



class BehaviorLoader {
public:
    // Scan folder for .audio (or .yaml) files and build raw behaviors
    static std::vector<BehaviorDef> LoadAudioBehaviorsFromFolder(const std::string& folderPath);

    static void ReplaceRefs(Node* n, const std::unordered_map<std::string, Node*>& defs);

};