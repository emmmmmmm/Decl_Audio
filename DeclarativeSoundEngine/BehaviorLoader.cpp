// BehaviorLoader.cpp
#include "pch.h"
#include "BehaviorLoader.hpp"
#include "BehaviorDef.hpp"
#include "ParserUtils.hpp"
#include "Log.hpp"
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <numeric>
#include <cstdlib>
std::vector<BehaviorDef> BehaviorLoader::LoadAudioBehaviorsFromFolder(const std::string& folderPath) {

	std::vector<YAML::Node> yamlBehaviors;

	namespace fs = std::filesystem;


	if (!fs::exists(folderPath)) {
		LogMessage("[BL][Error] folder not found: " + folderPath, LogCategory::BehaviorLoader, LogLevel::Warning);
		return {};
	}
	if (!fs::is_directory(folderPath)) {
		LogMessage("[BL][Error] not a directory: " +folderPath , LogCategory::BehaviorLoader, LogLevel::Warning);
		return {};
	}

	for (auto& entry : std::filesystem::directory_iterator(folderPath)) {
		if (!entry.is_regular_file()) continue;
		const auto ext = entry.path().extension().string();
		if (ext == ".audio" || ext == ".yaml") {
			try {
				YAML::Node root = YAML::LoadFile(entry.path().string());
				LogMessage(
					"[BehaviorLoader] loading file " + entry.path().filename().string() + " -> " +
					(root.IsSequence() ? "sequence of " + std::to_string(root.size()) + "" :
						"single map"),
					LogCategory::BehaviorLoader, LogLevel::Debug
				);
				if (root.IsSequence()) {
					for (auto node : root) yamlBehaviors.push_back(node);
				}
				else {
					yamlBehaviors.push_back(root);
				}
			}
			catch (const std::exception& e) {
				LogMessage("YAML parse error in " + entry.path().string() + ": " + e.what(),
					LogCategory::BehaviorLoader, LogLevel::Error);
			}
		}
	}

	// Register and parse BehaviorDefs
	ParserUtils::Context ctx;
	struct RawDef { std::string id; YAML::Node node; };
	std::vector<RawDef> rawDefs;
	rawDefs.reserve(yamlBehaviors.size());
	for (auto& beh : yamlBehaviors) {
		rawDefs.push_back({ beh["id"].as<std::string>(""), beh });
	}

	std::vector<std::unique_ptr<BehaviorDef>> definitions;
	definitions.reserve(rawDefs.size());

	/*
	for (auto& rd : rawDefs) {
		std::cerr << "[Debug] rd.id = " << rd.id << "\n";
		std::cerr << "[Debug] rd.node =\n" << rd.node << "\n\n";
	}
	*/
	

	for (auto& rd : rawDefs) {
		auto def = std::make_unique<BehaviorDef>();
		def->name = rd.id;
		def->busIndex = rd.node["bus"].as<int>(0);




		// pull matchTags
		if (auto tagsN = rd.node["matchTags"]) {
			if (!tagsN.IsSequence()) {
				LogMessage("[BehaviorLoader] warning: matchTags for " + def->name + " is not a sequence", LogCategory::BehaviorLoader, LogLevel::Warning);
			}
			else {
				for (const auto& t : tagsN) {
					def->matchTags.push_back(t.as<std::string>());
				}
			}
		}
		// build your internal matchConditions (whatever that is in your code)
		if (auto condsN = rd.node["matchConditions"]) {
			if (!condsN.IsSequence()) {
				LogMessage("[BehaviorLoader] warning: matchConditions for " + def->name + " is not a sequence", LogCategory::BehaviorLoader, LogLevel::Warning);
			}
			else {
				for (const auto& t : condsN) {
					def->matchConditions.push_back(Condition(t.as<std::string>()));
				}
			}
		}


		// parse onStart
		{
			auto nodeY = rd.node["onStart"];
			if (nodeY) {
				def->onStart.reset(ParserUtils::ParseNode(nodeY, ctx));
			}

		}

		// parse onActive
		{
			auto yamlN = rd.node["onActive"];
			

			if (yamlN.IsDefined() && !yamlN.IsNull()) {
				Node* parsed = ParserUtils::ParseNode(yamlN, ctx);

				if (!parsed)
					LogMessage("ParseNode(onActive) returned nullptr for " + rd.id, LogCategory::BehaviorLoader, LogLevel::Warning);
				else 
					def->onActive.reset(parsed);
				
			}
		}

		// parse onEnd
		{
			auto nodeY = rd.node["onEnd"];
			if (nodeY) 
				def->onEnd.reset(ParserUtils::ParseNode(nodeY, ctx));
		}

		if (def->onStart)
			def->onStart->PrintChildren();
		if (def->onActive)
			def->onActive->PrintChildren();
		if (def->onEnd)
			def->onEnd->PrintChildren();

		definitions.push_back(std::move(def));
	}


	// Resolve references
	for (auto& pr : ctx.unresolvedRefs) {
		auto it = ctx.definitions.find(pr.second);
		if (it != ctx.definitions.end()) pr.first->resolve(it->second);
		else LogMessage("Unresolved reference '" + pr.second + "'", LogCategory::BehaviorLoader, LogLevel::Error);
	}
	ctx.unresolvedRefs.clear();

	// Build BehaviorDef list
	std::vector<BehaviorDef> behaviors;
	behaviors.reserve(definitions.size());
	for (auto& defPtr : definitions) {
		BehaviorDef b;
		b.name = defPtr->name;
		b.id = rand();
		b.busIndex = defPtr->busIndex;
		b.matchTags = defPtr->matchTags;
		b.matchConditions = defPtr->matchConditions;
		b.rootVolume = defPtr->rootVolume;
		b.onStart = std::move(defPtr->onStart);
		b.onActive = std::move(defPtr->onActive);
		b.onEnd = std::move(defPtr->onEnd);
		behaviors.push_back(std::move(b));
	}
	LogMessage(
		std::to_string(behaviors.size()) + " behaviors loaded: ",
		LogCategory::BehaviorLoader, LogLevel::Debug
	);

	// Dump all Behaviors
	/*
	for (auto& b : behaviors) {
		std::cerr << "[Debug] BehaviorDef '" << b.name << "'\n";
		// dump matchTags
		std::cerr << "        matchTags       = { ";
		for (size_t i = 0; i < b.matchTags.size(); ++i) {
			std::cerr << b.matchTags[i];
			if (i + 1 < b.matchTags.size()) std::cerr << ", ";
		}
		std::cerr << " }\n";
		// dump matchConditions 
		std::cerr << "        matchConditions = { ";
		for (size_t i = 0; i < b.matchConditions.size(); ++i) {
			std::cerr << b.matchConditions[i].text;
			if (i + 1 < b.matchConditions.size()) std::cerr << ", ";
		}
		std::cerr << " }\n\n";
	}
	*/

	return behaviors;
}
