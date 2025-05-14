// ParserUtils.hpp
#pragma once

#include <yaml-cpp/yaml.h>
#include <optional>
#include <string>
#include "Node.hpp"

namespace ParserUtils {

    // Context passed during parsing, holding parameter references, tag maps, etc.
    struct Context {
        // Example fields:
        // std::unordered_map<std::string, float> parameters;
        // TagMap tagMap;
        // Definitions for id-based reference nodes (populated before node parsing)
        std::unordered_map<std::string, Node*> definitions;

        // Unresolved reference placeholders collected during first pass
        std::vector<std::pair<ReferenceNode*, std::string>> unresolvedRefs;
    };

    // Holds modifier values extracted from YAML: volume, pitch, loop flag
    struct ModifierMap {
        std::optional<std::string> volume;
        std::optional<std::string> pitch;
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
