// entity.hpp
#pragma once
#include <string>
#include "TagMap.hpp"
#include "LeafBuilder.hpp"
#include "ValueMap.hpp"
#include "ActiveBehavior.hpp"
#include "BehaviorDef.hpp"
#include "Vec3.hpp"




class Entity {
	std::string id{};
	TagMap tags{};
	ValueMap values{};
	std::vector<ActiveBehavior> activeBehaviors{};
	public:
		void TransitionToPhase(ActiveBehavior& ab, ActiveBehavior::Phase phase, AudioConfig* deviceCfg, AudioBufferManager* bufferManager) const;
			/*checks if we need to spawn new behaviors, stop old ones, and/or update active ones. */
		void Update(std::vector<BehaviorDef>& allDefs, const TagMap& globalTags, const ValueMap& globalValues, AudioConfig* deviceCfg, AudioBufferManager* bufferManager);

		void SetTag(const std::string& tag)					{ tags.AddTag(tag); }
		void SetTransientTag(const std::string& tag)		{ tags.AddTag(tag, true); }
		void ClearTransientTag(const std::string& tag)		{ tags.ClearTransient(); }
		void ClearTag(const std::string& tag)				{ tags.RemoveTag(tag); }
		void SetValue(const std::string& key, Value value)	{ values.SetValue(key, value); }
		void ClearValue(const std::string& key)				{ values.ClearValue(key); }
		void SetPosition(float x, float y, float z)			{ values.SetValue("position", Vec3(x, y, z)); }

		TagMap& GetTags()									{ return tags; }
		ValueMap& GetValues()								{ return values; }
		std::vector<ActiveBehavior>& GetBehaviors() { return activeBehaviors; }
		/*returns a vector of leaves to be stored in the tripplebuffer for mixing*/
		// that's bs, we would want to return the voices instead, ... right? ...riiiight?

		std::vector<LeafBuilder::Leaf> BuildLeaves(AudioConfig* deviceCfg, AudioBufferManager* bufferManager);
		void SyncBehaviors(std::vector<BehaviorDef>& allDefs, const TagMap& globalTags, const ValueMap& globalValues, AudioConfig* deviceCfg, AudioBufferManager* bufferManager);

	//Snapshot GenerateSnapshot();
};

