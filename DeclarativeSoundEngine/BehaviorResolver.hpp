#pragma once
#include "RawAudioBehavior.hpp"
#include <vector>

class BehaviorResolver {
public:
    // Merge parent and child ASTs into a single AST
    static ASTNode mergeBehaviorAST(const ASTNode& parent, const ASTNode& child);

    // Entry: list of all loaded raw behaviors
    // Performs inheritance and overrides resolution in-place
    static void resolveAll(std::vector<RawAudioBehavior>& behaviors);
};

static ASTNode mergeAST(const ASTNode& base, const ASTNode& ov);

static std::string evaluateRelative(const std::string& baseStr, const std::string& expr);
