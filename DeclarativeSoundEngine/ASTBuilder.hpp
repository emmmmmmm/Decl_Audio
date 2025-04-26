#pragma once
#include "ASTNode.hpp"
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <unordered_map>

// Builds an ASTNode tree from a YAML::Node
class ASTBuilder {
public:
    static ASTNode build(const YAML::Node& node) {
        ASTNode out;
        if (node.IsScalar()) {
            out.type = ASTNode::Type::Scalar;
            out.scalar = node.as<std::string>();
        }
        else if (node.IsSequence()) {
            out.type = ASTNode::Type::Seq;
            for (const auto& child : node) {
                out.seq.push_back(build(child));
            }
        }
        else if (node.IsMap()) {
            out.type = ASTNode::Type::Map;
            for (auto it = node.begin(); it != node.end(); ++it) {
                std::string key = it->first.as<std::string>();
                out.map.emplace(key, build(it->second));
            }
        }
        return out;
    }
};