#include "pch.h"
#include "BehaviorDefinitionManager.hpp"
#include "BehaviorLoader.hpp"
#include <iostream>
#include "Log.hpp"



void BehaviorDefinitionManager::LoadFilesFromFolder(const std::string& path)
{

	auto behaviors = BehaviorLoader::LoadAudioBehaviorsFromFolder(path);
	matchdefs.clear();
	matchdefs.reserve(behaviors.size());
	playdefs.clear();
	playdefs.reserve(behaviors.size());

	for (auto& b : behaviors) {
		if (!b.onStart && !b.onActive && !b.onEnd) { // we shouldn't actually continue here I think, but instead treat them as b.onstart?
			LogMessage(
				"BehaviorDefinitionManager: skipping behavior '" + b.name +
				"'—no soundNode or soundName defined",
				LogCategory::BehaviorDefMgr,
				LogLevel::Warning
			);
			continue;
		}

		// 1) Build MatchDefinition
		MatchDefinition md;
		md.id = b.id;
		md.name = b.name;
		md.matchTags = b.matchTags;
		md.matchConditions = b.matchConditions;
		md.parameters = b.parameters;
		matchdefs.push_back(std::move(md));

		
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

		LogMessage("BehaviorDefinitionManager: loaded " +
			b.name + " valid behaviors",
			LogCategory::BehaviorDefMgr,
			LogLevel::Info);
	}

	LogMessage("BehaviorDefinitionManager: loaded " +
		std::to_string(playdefs.size()) + " valid behaviors",
		LogCategory::BehaviorDefMgr,
		LogLevel::Info);
}


std::vector<PlayDefinition>& BehaviorDefinitionManager::GetPlayDefs()
{
	return playdefs;
}

std::vector<MatchDefinition>& BehaviorDefinitionManager::GetMatchDefs()
{
	return matchdefs;
}
