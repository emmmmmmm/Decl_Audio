#include "pch.h"
#include "Entity.hpp"
#include "Log.hpp"
#include <algorithm>
#include "TagMap.hpp"
#include "ValueMap.hpp"
#include "ActiveBehavior.hpp"
#include "MatchUtils.hpp"

void Entity::TransitionToPhase(ActiveBehavior& ab, ActiveBehavior::Phase phase, AudioConfig* deviceCfg, AudioBufferManager* bufferManager)const
{	// stop all voices from current graph
	ab.StopAllVoices(); 
	
	

	// set new phase & graph
	switch (phase) {
		case ActiveBehavior::Phase::Start: {
			ab.SetPhase(ActiveBehavior::Phase::Start);
			auto newGraph = ab.GetDefinition()->onStart;
			ab.currentNodeGraph = newGraph ? newGraph->clone() : nullptr;
			LogMessage(ab.Name()+ ": Transition to Phase::Start", LogCategory::Entity, LogLevel::Debug);
			break;
		}
		case ActiveBehavior::Phase::Active: {
			ab.SetPhase(ActiveBehavior::Phase::Active);
			auto newGraph = ab.GetDefinition()->onActive;
			ab.currentNodeGraph = newGraph ? newGraph->clone() : nullptr;
			LogMessage(ab.Name()+ ": Transition to Phase::Active", LogCategory::Entity, LogLevel::Debug);
			break;
		}
		case ActiveBehavior::Phase::Ending: {
			ab.SetPhase(ActiveBehavior::Phase::Ending);
			auto newGraph = ab.GetDefinition()->onEnd;
			ab.currentNodeGraph = newGraph ? newGraph->clone() : nullptr;
			LogMessage(ab.Name()+ ": Transition to Phase::Ended", LogCategory::Entity, LogLevel::Debug);
			break;
		}
		case ActiveBehavior::Phase::Finished: {
			ab.SetPhase(ActiveBehavior::Phase::Finished);
			LogMessage(ab.Name() + ": Transition to Phase::Finished", LogCategory::Entity, LogLevel::Debug);
			ab.currentNodeGraph = nullptr;
			return; // nothing to do here
		}
	}

	// start voices for new graph
	
	std::vector<LeafBuilder::Leaf> leaves;
	LeafBuilder::BuildLeaves(ab.currentNodeGraph.get(), values, 0, false, busId, leaves, deviceCfg, bufferManager); // TODO: pass correct params

	for (auto& leaf : leaves) {

		// start new voice
		Voice v;
		v.buffer = leaf.buffer;
		v.playhead = 0;
		v.loop = leaf.loop;
		v.busIndex = leaf.bus; 
		v.source = leaf.src;
		v.startSample = leaf.startSample;
		v.currentVol = leaf.volume(values);

		LogMessage("[start voice], loop: " + std::to_string(leaf.loop)
			+ " offset: " + std::to_string(leaf.startSample)
			+ " volume: " + std::to_string(v.currentVol), LogCategory::Entity, LogLevel::Debug);


		ab.AddVoice(std::move(v));
	}
}

void Entity::Update(std::vector<BehaviorDef>& allDefs, const TagMap& globalTags, const ValueMap& globalValues, AudioConfig* deviceCfg, AudioBufferManager* bufferManager)
{
	
	// update all active behaviors
	// UpdateBehaviors(); // sth like this
	// where for each behavior we 
	//	- update the statemachine, 
	//  - create leaves, 
	//  - check if they are already playing, 
	//  - and if not, start them
	//  - remove the stopped (or silent?) ones - will be removed in audiomanager


	SyncBehaviors(allDefs, globalTags, globalValues, deviceCfg, bufferManager);
	tags.ClearTransient();

	if (activeBehaviors.empty())
		return;

	for (auto& ab : activeBehaviors) {

		ab.RemoveFinishedVoices();

		switch (ab.GetPhase()) {
		case ActiveBehavior::Phase::Init:
		{
			TransitionToPhase(ab, ActiveBehavior::Phase::Start, deviceCfg, bufferManager);
			break;
		}
		case ActiveBehavior::Phase::Start:
		{
			if (ab.AllVoicesFinished()) {
				TransitionToPhase(ab,ActiveBehavior::Phase::Active,deviceCfg,bufferManager);
			}
			break;
		}

		case ActiveBehavior::Phase::Active:
		{
			if (ab.AllVoicesFinished()) {
				TransitionToPhase(ab, ActiveBehavior::Phase::Ending, deviceCfg, bufferManager);
			}

			break;
		}
		case ActiveBehavior::Phase::Ending:
		{
			if (ab.AllVoicesFinished()) {
				TransitionToPhase(ab, ActiveBehavior::Phase::Finished, deviceCfg, bufferManager);
			}
			break;
		}
		case ActiveBehavior::Phase::Finished:
		default:
			continue;
		}

		std::vector<LeafBuilder::Leaf> leaves;
		LeafBuilder::BuildLeaves(ab.currentNodeGraph.get(), values, 0, false, 0, leaves, deviceCfg, bufferManager); // TODO: pass correct params

		for (auto& leaf : leaves) {
			auto voice = ab.FindVoiceForLeaf(leaf); // TODO: find a better way...

			
			if (!voice) {
				// should no longer happen
				// actually, this might still be necessary, eg for sequence/looping nodes? - not sure tbh.
				LogMessage("Could not find voice for: " + leaf.src->sound, LogCategory::Entity, LogLevel::Warning);
				continue;
			}

			voice->targetVol = leaf.volume(values);
			voice->targetPitch = leaf.pitch(values);

		}
	}
}

void Entity::SyncBehaviors(std::vector<BehaviorDef>& allDefs, const TagMap& globalTags, const ValueMap& globalValues, AudioConfig* deviceCfg, AudioBufferManager* bufferManager)
{
	// Build name -> BehaviorDef* map (just once)
	std::unordered_map<std::string, BehaviorDef*> defMap;
	for (auto& def : allDefs)
		defMap[def.name] = &def;

	// Gather all matching definitions
	// there has to be a smarter way to do this, this is ... really bad.
	std::unordered_set<std::string> desired;
	for (const auto& def : allDefs) {
		auto score = MatchUtils::MatchScore(def, tags, globalTags, values, globalValues);

		
		if (score >= 0) {
			desired.insert(def.name);

		}
	}


	// TODO: think about how we might be able to optimize this whole matching block!
	// maybe we don't actually have to check everything every tick?
	// I feel like there could be a more "analytical" approach instead of the full rotation

	
	// Step 2: Get active names
	std::unordered_set<std::string> activeNames;
	for (const auto& ab : activeBehaviors)
		activeNames.insert(ab.GetDefinition()->name);

	// Step 3: Diff for toSpawn, toRetire, toUpdate
	std::vector<BehaviorDef*> toSpawn;
	for (const auto& name : desired)
		if (!activeNames.count(name))
			toSpawn.push_back(defMap[name]);

	std::vector<ActiveBehavior*> toRetire;
	for (auto& ab : activeBehaviors)
		if (!desired.count(ab.GetDefinition()->name))
			toRetire.push_back(&ab);

	/*std::vector<std::pair<BehaviorDef*, ActiveBehavior*>> toUpdate;
	for (auto& ab : activeBehaviors)
		if (desired.count(ab.GetDefinition()->name))
			toUpdate.emplace_back(defMap[ab.GetDefinition()->name], &ab);*/

	// Do ... things.
	for (auto& def : toSpawn) {
		LogMessage("starting new behavior: " + def->name, LogCategory::Entity, LogLevel::Debug);
		auto& ab = activeBehaviors.emplace_back(*ActiveBehavior::Create(def, 0)); // TODO get from factory instead of creating


	}
	for (auto& ab : toRetire) {
		if (ab->GetPhase() !=ActiveBehavior::Phase::Active) // only active state actually needs to be ended (in case it's looping) 
			continue;
		LogMessage("stopping behavior: "+ ab->GetDefinition()->name, LogCategory::Entity, LogLevel::Debug);
		TransitionToPhase(*ab, ActiveBehavior::Phase::Ending, deviceCfg, bufferManager);

	}

	// Step 4: Remove finished behaviors
	activeBehaviors.erase(
		std::remove_if(
			activeBehaviors.begin(),
			activeBehaviors.end(),
			[](const ActiveBehavior& ab) {
				return ab.GetPhase() == ActiveBehavior::Phase::Finished;
			}
		),
		activeBehaviors.end()
	);

	// if activeBehaviors is empty, we should remove this entity
}