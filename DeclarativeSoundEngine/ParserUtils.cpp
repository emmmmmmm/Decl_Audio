// ParseNode.cpp
#include "pch.h"
#include "Node.hpp"
#include "ASTNode.hpp"
#include "Condition.hpp"
#include "Expression.hpp"

std::unique_ptr<Node> ParseNode(const ASTNode& ast) {
    if (!ast.isMap()) return nullptr;
    auto it = ast.map.begin();
    const std::string nodeType = it->first;
    const ASTNode& payload = it->second;
    std::unique_ptr<Node> node;

    if (nodeType == "sound") {
        auto sn = std::make_unique<SoundNode>();
        sn->sound = payload.scalar;
        node = std::move(sn);
    }
    else if (nodeType == "layer") {
        auto ln = std::make_unique<LayerNode>();
        if (auto soundsIt = payload.map.find("sounds");
            soundsIt != payload.map.end() && soundsIt->second.isSeq()) {
            for (auto& childAst : soundsIt->second.seq) {
                ln->children.push_back(ParseNode(childAst));
            }
        }
        node = std::move(ln);
    }
    else if (nodeType == "random") {
        auto rn = std::make_unique<RandomNode>();
        if (auto soundsIt = payload.map.find("sounds");
            soundsIt != payload.map.end() && soundsIt->second.isSeq()) {
            for (auto& childAst : soundsIt->second.seq) {
                rn->children.push_back(ParseNode(childAst));
            }
        }
        node = std::move(rn);
    }
    else if (nodeType == "blend") {
        auto bn = std::make_unique<BlendNode>();
        if (auto pIt = payload.map.find("parameter");
            pIt != payload.map.end() && pIt->second.isScalar()) {
            bn->parameter = pIt->second.scalar;
        }
        if (auto bIt = payload.map.find("blends");
            bIt != payload.map.end() && bIt->second.isSeq()) {
            for (auto& pointAst : bIt->second.seq) {
                float at = std::stof(pointAst.map.at("at").scalar);
                ASTNode childNode = pointAst;
                if (pointAst.map.count("soundNode"))
                    childNode = pointAst.map.at("soundNode");
                bn->blends.push_back({ at, ParseNode(childNode) });
            }
        }
        node = std::move(bn);
    }
    else if (nodeType == "select") {
        auto sn = std::make_unique<SelectNode>();
        if (auto pIt = payload.map.find("parameter");
            pIt != payload.map.end() && pIt->second.isScalar()) {
            sn->parameter = pIt->second.scalar;
        }
        if (auto oIt = payload.map.find("options");
            oIt != payload.map.end() && oIt->second.isMap()) {
            for (auto& kv : oIt->second.map) {
                SelectNode::Option opt;
                opt.match = kv.first;
                opt.node = ParseNode(kv.second);
                sn->options.push_back(std::move(opt));
            }
        }
        node = std::move(sn);
    }

    // common params
    if (ast.map.count("volume"))   node->volume = std::stof(ast.map.at("volume").scalar);
    if (ast.map.count("pitch"))    node->pitch = std::stof(ast.map.at("pitch").scalar);
    if (ast.map.count("fadeIn"))   node->fadeIn = std::stof(ast.map.at("fadeIn").scalar);
    if (ast.map.count("fadeOut"))  node->fadeOut = std::stof(ast.map.at("fadeOut").scalar);
    if (ast.map.count("delay"))    node->delay = std::stof(ast.map.at("delay").scalar);
    if (ast.map.count("loop"))     node->loop = std::stoi(ast.map.at("loop").scalar);

    return node;
}
Condition parseCondition(const std::string& string)
{
    return Condition(string);
}

Expression parseExpression(const std::string& string)
{
    return Expression(string);
}
