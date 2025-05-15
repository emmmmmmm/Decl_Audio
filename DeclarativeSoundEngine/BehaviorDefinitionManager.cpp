#include "pch.h"
#include "BehaviorDefinitionManager.hpp"
#include "BehaviorLoader.hpp"
#include <iostream>
#include "Log.hpp"



void BehaviorDefinitionManager::LoadFilesFromFolder(const std::string& path)
{

	auto behaviors = BehaviorLoader::LoadAudioBehaviorsFromFolder(path);
	audioBehaviors = behaviors;




	// all of this will be obsolete! // TODO
	matchdefs.clear();
	matchdefs.reserve(behaviors.size());
	playdefs.clear();
	playdefs.reserve(behaviors.size());

	for (auto& b : behaviors) {
		if (!b.onStart && !b.onActive && !b.onEnd) {
			LogMessage(
				"BehaviorDefinitionManager: skipping behavior " + b.name +
				" - no soundNode or soundName defined",
				LogCategory::BehaviorDefMgr,
				LogLevel::Warning
			);
			continue;
		}

		//LogMessage("BehaviorDefinitionManager: building MD: "+b.name, LogCategory::BehaviorDefMgr, LogLevel::Info);
		// 1) Build MatchDefinition
		MatchDefinition md;
		md.id = b.id;
		md.name = b.name;
		md.matchTags = b.matchTags;
		md.matchConditions = b.matchConditions;
		md.parameters = b.parameters;
		matchdefs.push_back(std::move(md));
		//LogMessage("BehaviorDefinitionManager: building PD: "+b.name, LogCategory::BehaviorDefMgr, LogLevel::Info);

		// 2) Build PlayDefinition
		PlayDefinition pd;
		pd.id = b.id;
		pd.name = b.name;
		if (b.onStart)
			pd.onStart = b.onStart->clone();
		if (b.onActive)
			pd.onActive = b.onActive->clone();
		if (b.onEnd) 
			pd.onEnd = b.onEnd->clone();


		playdefs.push_back(std::move(pd));

		LogMessage("BehaviorDefinitionManager: loaded Behavior: " +
			b.name ,
			LogCategory::BehaviorDefMgr,
			LogLevel::Info);
	}

	LogMessage("BehaviorDefinitionManager: loaded " +
		std::to_string(playdefs.size()) + " valid behaviors",
		LogCategory::BehaviorDefMgr,
		LogLevel::Info);


	std::cerr << "[Debug] Registered MatchDefinitions:\n";
	for (auto& md : matchdefs) {
		std::cerr << "  Behavior " << md.name << " -> tags={";
		for (auto& t : md.matchTags) std::cerr << t << ",";
		std::cerr << "}  conds={";
		for (auto& c : md.matchConditions)    std::cerr << c.text << ",";
		std::cerr << "}\n";
	}


}


std::vector<PlayDefinition>& BehaviorDefinitionManager::GetPlayDefs()
{
	return playdefs;
}

std::vector<MatchDefinition>& BehaviorDefinitionManager::GetMatchDefs()
{
	return matchdefs;
}
