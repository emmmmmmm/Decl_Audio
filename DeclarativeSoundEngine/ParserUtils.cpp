// ParseNode.cpp
#include "pch.h"
#include "Node.hpp"
#include "ASTNode.hpp"
#include "Condition.hpp"
#include "Expression.hpp"
#include <iostream>
#include "Log.hpp"

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
				std::unique_ptr<Node> child;
				if (childAst.isMap()) {
					child = ParseNode(childAst);

				}
				else if (childAst.isScalar()) {
					// scalar shorthand: just a filename
					auto sn = std::make_unique<SoundNode>();
					sn->sound = childAst.scalar;
					child = std::move(sn);

				}
				if (child) {
					ln->children.push_back(std::move(child));

				}

			}
		}
		node = std::move(ln);
	}
	else if (nodeType == "random") {
		auto rn = std::make_unique<RandomNode>();
		if (auto soundsIt = payload.map.find("sounds");
			soundsIt != payload.map.end() && soundsIt->second.isSeq()) {
			for (auto& childAst : soundsIt->second.seq) {
				std::unique_ptr<Node> child;
				if (childAst.isMap()) {
					child = ParseNode(childAst);

				}
				else if (childAst.isScalar()) {
					auto sn = std::make_unique<SoundNode>();
					sn->sound = childAst.scalar;
					child = std::move(sn);

				}
				if (child) {
					rn->children.push_back(std::move(child));

				}

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
			// sort blendpoints (?) // think about if this is actually a good idea though, because it might create create inconsistencies!
			// std::sort(bn->blends.begin(), bn->blends.end(),
			// 		[](const BlendNode::Point& a, const BlendNode::Point& b)
			// 		{ return a.at < b.at; });
		}
		node = std::move(bn);
	}
	else if (nodeType == "select") {
		auto sel = std::make_unique<SelectNode>();
		sel->parameter = payload.map.at("parameter").scalar;

		if (auto cases = payload.map.find("cases"); cases != payload.map.end()) {
			for (auto& kv : cases->second.map) {
				SelectNode::Option opt;
				opt.pattern = kv.first;
				opt.node = ParseNode(kv.second);
				sel->options.push_back(std::move(opt));
			}
		}
		if (auto def = payload.map.find("default"); def != payload.map.end())
			sel->defaultNode = ParseNode(def->second);

		node = std::move(sel);
	}

	// common params
	if (ast.map.count("volume"))   node->volume = Expression(ast.map.at("volume").scalar);
	if (ast.map.count("pitch"))    node->pitch = std::stof(ast.map.at("pitch").scalar);
	if (ast.map.count("fadeIn"))   node->fadeIn = std::stof(ast.map.at("fadeIn").scalar);
	if (ast.map.count("fadeOut"))  node->fadeOut = std::stof(ast.map.at("fadeOut").scalar);
	if (ast.map.count("delay"))    node->delay = std::stof(ast.map.at("delay").scalar);
	if (ast.map.count("loop"))     node->loop = std::stoi(ast.map.at("loop").scalar);


	// spatial params
	// maybe we want to use this in the future for a positional offset against the entity?
	/*
	if (ast.map.count("position")) {
		const ASTNode& posNode = ast.map.at("position");
		if (posNode.isSeq() && posNode.seq.size() == 3) {
			node->position = std::array<float, 3>{
			std::stof(posNode.seq[0].scalar),
			std::stof(posNode.seq[1].scalar),
			std::stof(posNode.seq[2].scalar)
			};
		}
		else {
			// you can hook into your logging system here
			LogMessage("Warning: 'position' must be a sequence of 3 scalars\n", LogCategory::SoundManager, LogLevel::Warning);
		}
	}
	*/
	if (ast.map.count("radius")) {
		node->radius = std::stof(ast.map.at("radius").scalar);

	}




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
