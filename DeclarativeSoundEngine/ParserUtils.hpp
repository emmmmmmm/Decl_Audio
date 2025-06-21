// ParserUtils.hpp
#pragma once

#include <yaml-cpp/yaml.h>
#include <optional>
#include <string>
#include "Node.hpp"

namespace ParserUtils {

    struct Context {
        
        std::unordered_map<std::string, Node*>              definitions;
        std::vector<std::pair<ReferenceNode*, std::string>> unresolvedRefs;
    };

    struct ModifierMap {
        std::optional<std::string> volume;
        std::optional<std::string> pitch;
        std::optional<float> radius;
        bool loop = false;
    };

    // Extract the primary node type key (sound, random, sequence, blend, select, delay, loop, parallel)
    std::string ExtractCoreKey(const YAML::Node& mapNode);

    // Extract modifiers (volume, pitch, loop flag)
    ModifierMap ExtractModifiers(const YAML::Node& mapNode);

    // Extract children nodes: for containers (nodes list, inline sequences, blend/select cases)
    YAML::Node ExtractChildren(const YAML::Node& mapNode);

    // Parse a YAML node into a Node* AST
    Node* ParseNode(const YAML::Node& yamlNode,  Context& ctx);

    // Normalize nested loop nodes by collapsing redundant wrappers
    Node* NormalizeLoops(Node* root);

} // namespace ParserUtils
