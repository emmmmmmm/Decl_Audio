#pragma once
#include "ASTNode.hpp"
#include <string>

// Holds the unmerged AST for one .audio file
struct RawAudioBehavior {
    std::string id;         // behavior ID
    ASTNode     root;       // top-level AST map of all fields
};
