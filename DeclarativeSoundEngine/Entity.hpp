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
public:
	std::string id{};

	TagMap tags{};
	ValueMap values{};

	int busId{};
	std::vector<ActiveBehavior> activeBehaviors{};

	void TransitionToPhase(ActiveBehavior& ab, ActiveBehavior::Phase phase, AudioConfig* deviceCfg, AudioBufferManager* bufferManager) const;
	void Update(std::vector<BehaviorDef>& allDefs, const TagMap& globalTags, const ValueMap& globalValues, AudioConfig* deviceCfg, AudioBufferManager* bufferManager);
	void SyncBehaviors(std::vector<BehaviorDef>& allDefs, const TagMap& globalTags, const ValueMap& globalValues, AudioConfig* deviceCfg, AudioBufferManager* bufferManager);

	void SetTag(const std::string& tag) { tags.AddTag(tag);  }
	void SetTransientTag(const std::string& tag) { tags.AddTag(tag, true);}
	void ClearTransientTag(const std::string& tag) { tags.ClearTransient(); }
	void ClearTag(const std::string& tag) { tags.RemoveTag(tag); }
	void SetValue(const std::string& key, Value value) { values.SetValue(key, value); }
	void ClearValue(const std::string& key) { values.ClearValue(key); }
	void SetPosition(float x, float y, float z) { values.SetValue("position", Vec3(x, y, z)); }

	TagMap& GetTags() { return tags; }
	ValueMap& GetValues() { return values; }
	std::vector<ActiveBehavior>& GetBehaviors() { return activeBehaviors; }

	
	inline void SetBus(int id) { busId = id; }
};

