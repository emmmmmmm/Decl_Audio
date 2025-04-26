#include "pch.h"
#include "BehaviorResolver.hpp"
#include "Log.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <unordered_set>

// Helper: deep-merge two ASTNodes (base + override)
static ASTNode mergeAST(const ASTNode& baseNode, const ASTNode& overrideNode) {
    ASTNode out = baseNode;
    for (const auto& kv : overrideNode.map) {
        const auto& key = kv.first;
        const ASTNode& v = kv.second;
        if (v.isMap() && out.map.count(key) && out.map.at(key).isMap()) {
            out.map[key] = mergeAST(out.map.at(key), v);
        }
        else {
            out.map[key] = v;
        }
    }
    return out;
}

// Helper: evaluate relative scalar expressions like "+0.2", "*1.5"
static std::string evaluateRelative(const std::string& baseStr, const std::string& expr) {
    float baseVal = std::stof(baseStr);
    char op = expr[0];
    float num = std::stof(expr.substr(1));
    float result = baseVal;
    switch (op) {
    case '+': result = baseVal + num; break;
    case '-': result = baseVal - num; break;
    case '*': result = baseVal * num; break;
    default: break;
    }
    return std::to_string(result);
}

ASTNode BehaviorResolver::mergeBehaviorAST(const ASTNode& parent, const ASTNode& child) {
    ASTNode out = parent;
    for (const auto& kv : child.map) {
        const auto& key = kv.first;
        const ASTNode& cv = kv.second;
        if (key == "inherit") continue;
        if (key == "overrides" && cv.isMap()) {
            out = mergeAST(out, cv);
        }
        else {
            auto pit = parent.map.find(key);
            if (pit == parent.map.end()) {
                out.map[key] = cv;
            }
            else {
                const ASTNode& pv = pit->second;
                if (pv.isSeq() && cv.isSeq()) {
                    ASTNode merged = pv;
                    merged.seq.insert(merged.seq.end(), cv.seq.begin(), cv.seq.end());
                    out.map[key] = std::move(merged);
                }
                else if (pv.isScalar() && cv.isScalar()) {
                    const auto& cs = cv.scalar;
                    if (cs.size() > 1 && (cs[0] == '+' || cs[0] == '-' || cs[0] == '*')) {
                        out.map[key].scalar = evaluateRelative(pv.scalar, cs);
                    }
                    else {
                        out.map[key] = cv;
                    }
                }
                else {
                    out.map[key] = cv;
                }
            }
        }
    }
    return out;
}

void BehaviorResolver::resolveAll(std::vector<RawAudioBehavior>& behaviors) {
    // Build AST + parent map
    struct NodeInfo { ASTNode ast; std::string parentId; };
    std::unordered_map<std::string, NodeInfo> info;
    info.reserve(behaviors.size());
    for (auto& b : behaviors) {
        NodeInfo ni{ b.root, "" };
        auto it = b.root.map.find("inherit");
        if (it != b.root.map.end() && it->second.isScalar())
            ni.parentId = it->second.scalar;
        info[b.id] = std::move(ni);
    }

    // Build graph
    std::unordered_map<std::string, std::vector<std::string>> children;
    std::unordered_map<std::string, int> indegree;
    for (auto& kv : info) indegree[kv.first] = 0;
    for (auto& kv : info) {
        const auto& id = kv.first;
        const auto& pid = kv.second.parentId;
        if (pid.empty()) continue;
        if (!info.count(pid)) {
            LogMessage("[BehaviorResolver] Unknown parent '" + pid + "' for '" + id + "'", LogCategory::BehaviorLoader, LogLevel::Warning);
            continue;
        }
        children[pid].push_back(id);
        indegree[id]++;
    }

    // Topo sort
    std::queue<std::string> q;
    for (auto& kv : indegree) if (kv.second == 0) q.push(kv.first);
    std::vector<std::string> sorted;
    while (!q.empty()) {
        auto u = q.front();q.pop();
        sorted.push_back(u);
        for (auto& v : children[u]) {
            if (--indegree[v] == 0) q.push(v);
        }
    }

    // Remove cycles
    if (sorted.size() != info.size()) {
        LogMessage("[BehaviorResolver] Circular inheritance detected", LogCategory::BehaviorLoader, LogLevel::Error);
        std::unordered_set<std::string> ok(sorted.begin(), sorted.end());
        behaviors.erase(
            std::remove_if(behaviors.begin(), behaviors.end(),
                [&](auto const& b) {return ok.find(b.id) == ok.end();}),
            behaviors.end());
    }

    // Merge in order
    for (auto const& id : sorted) {
        auto& ni = info[id];
        if (ni.parentId.empty()) continue;
        ni.ast = mergeBehaviorAST(info[ni.parentId].ast, ni.ast);
    }

    // Strip metadata + write back
    for (auto& b : behaviors) {
        auto& ni = info[b.id];
        ni.ast.map.erase("inherit");
        ni.ast.map.erase("overrides");
        b.root = std::move(ni.ast);
    }
}