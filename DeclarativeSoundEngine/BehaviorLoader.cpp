#include "pch.h"
#include "BehaviorLoader.hpp"
#include "ASTBuilder.hpp"
#include "BehaviorResolver.hpp"
#include "AudioBehavior.hpp"
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include "Log.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <regex>
#include "ParserUtils.hpp"
# include "Node.hpp"
#include <objbase.h>
#include <iostream>
#include <sstream>

std::vector<AudioBehavior> BehaviorLoader::LoadAudioBehaviorsFromFolder(const std::string& folderPath) {
	// Step 1: parse raw ASTs
	std::vector<RawAudioBehavior> raws;
	for (auto& entry : std::filesystem::directory_iterator(folderPath)) {
		if (!entry.is_regular_file()) continue;
		auto path = entry.path();
		auto ext = path.extension().string();
		if (ext == ".audio" || ext == ".yaml") {
			try {
				YAML::Node yamlRoot = YAML::LoadFile(path.string());
				// Support both single-object and sequence of behaviors in one file
				std::vector<YAML::Node> yamlBehaviors;
				if (yamlRoot.IsSequence()) {
					for (const auto entryNode : yamlRoot) {
						yamlBehaviors.push_back(entryNode);
					}
				}
				else {
					yamlBehaviors.push_back(yamlRoot);
				}
				for (auto& behNode : yamlBehaviors) {
					ASTNode ast = ASTBuilder::build(behNode);
					RawAudioBehavior raw;
					// extract ID if present
					if (ast.map.count("id") && ast.map.at("id").isScalar()) {
						raw.id = ast.map.at("id").scalar;
					}
					else {
						raw.id = path.stem().string();
					}
					raw.root = std::move(ast);
					// Debug: raw parse complete
					LogMessage("[BehaviorLoader] Parsed raw behavior: " + raw.id,
						LogCategory::BehaviorLoader, LogLevel::Debug);
					raws.push_back(std::move(raw));
				}
			}
			catch (const std::exception& e) {
				LogMessage(std::string("YAML parse error in ") + path.string() + ": " + e.what(),
					LogCategory::BehaviorLoader, LogLevel::Error);
			}
		}
	}

	// Debug: dump raw AST field names and values
	
	/*
	for (auto& raw : raws) {
		auto& m = raw.root.map;
		LogMessage("[BehaviorLoader] Raw AST for '" + raw.id + "' has fields:", LogCategory::BehaviorLoader, LogLevel::Debug);
		for (auto& kv : m) {
			std::string key = kv.first;
			LogMessage("    key: " + key + ", type: " + (kv.second.isScalar() ? "Scalar" : kv.second.isSeq() ? "Sequence" : "Map"),
				LogCategory::BehaviorLoader, LogLevel::Debug);
			if (key == "parameters" && kv.second.isMap()) {
				for (auto& pkv : kv.second.map) {
					LogMessage("        param: " + pkv.first + " = " + pkv.second.scalar,
						LogCategory::BehaviorLoader, LogLevel::Debug);
				}
			}
			else if ((key == "matchTags" || key == "matchConditions") && kv.second.isSeq()) {
				for (auto& item : kv.second.seq) {
					LogMessage(std::string("        ") + key + ": " + item.scalar,
						LogCategory::BehaviorLoader, LogLevel::Debug);
				}
			}
		}
	}
	*/

	// Step 2: resolve inheritance/overrides (once)
	BehaviorResolver::resolveAll(raws);

	// Debug: dump resolved AST field names
	/*
	for (auto& raw : raws) {
		auto& m = raw.root.map;
		LogMessage("[BehaviorLoader] Resolved AST for '" + raw.id + "' has fields:", LogCategory::BehaviorLoader, LogLevel::Debug);
		for (auto& kv : m) {
			LogMessage(std::string("    key: ") + kv.first,
				LogCategory::BehaviorLoader, LogLevel::Debug);
		}
	}
	*/
	// Step 3: instantiate into final AudioBehavior structs
	std::vector<AudioBehavior> behaviors;
	behaviors.reserve(raws.size());
	for (auto& raw : raws) {
		AudioBehavior b;
		// is that ... smart?^^
		GUID guid;
		auto result = CoCreateGuid(&guid);
		b.id = guid.Data1;
		b.name = raw.id;
		// flatten matchTags
		if (auto it = raw.root.map.find("matchTags");
			it != raw.root.map.end() && it->second.isSeq()) {
			for (auto& tagAst : it->second.seq)
				b.matchTags.push_back(tagAst.scalar);
		}
		// flatten matchConditions
		if (auto it = raw.root.map.find("matchConditions");
			it != raw.root.map.end() && it->second.isSeq()) {
			for (auto& cAst : it->second.seq)
				b.matchConditions.push_back(parseCondition(cAst.scalar));
		}
		// flatten parameters
		if (auto it = raw.root.map.find("parameters");
			it != raw.root.map.end() && it->second.isMap()) {
			for (auto& kv : it->second.map)
				b.parameters[kv.first] = parseExpression(kv.second.scalar);
		}
		// build sound graph
		if (auto it = raw.root.map.find("soundNode");
			it != raw.root.map.end()) {
			b.rootSoundNode = ParseNode(it->second);
		}
		else if (auto it2 = raw.root.map.find("soundName");
			it2 != raw.root.map.end() && it2->second.isScalar()) {
			// Create a single SoundNode from the scalar shortcut
			auto snd = std::make_unique<SoundNode>();
			snd->sound = it2->second.scalar;
			b.rootSoundNode = std::move(snd);
		}


		// Debug: instantiated behavior summary
		LogMessage("[BehaviorLoader] Instantiated behavior '" + b.name + "': "
			"tags=" + std::to_string(b.matchTags.size()) + ", "
			"conds=" + std::to_string(b.matchConditions.size()) + ", "
			"params=" + std::to_string(b.parameters.size()),
			LogCategory::BehaviorLoader, LogLevel::Debug);
		behaviors.push_back(std::move(b));
	}

	return behaviors;
}
