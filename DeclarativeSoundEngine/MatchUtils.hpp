﻿// MatchUtils.hpp
#pragma once

#include <string>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <functional>


struct BehaviorDef;
class TagMap;
class ValueMap;
struct MatchDefinition;



namespace MatchUtils {

	// we might actually want one for only perfect matches as well...?
	bool PatternMatch(const std::string& pat, const std::string& val);

	int TagSpecificity(const std::string& tag);

	bool TagMatches(const std::string& pattern, const std::string& actual);

	int MatchScore(
		const BehaviorDef& def,
		const TagMap& tags,
		const TagMap& globalTags,
		const ValueMap& values,
		const ValueMap& globalValues);

	 void FindMatchingBehaviorsForTag(
		const std::string& tag, 
		std::unordered_map<std::string, 
		std::vector<BehaviorDef*>> exactMatchMap, 
		std::vector<std::pair<std::string, BehaviorDef*>> wildcardMatchers, 
		std::function<void(BehaviorDef*)> callback);

	inline std::string JoinTags(const std::vector<std::string>& tags) {
		std::ostringstream oss;
		oss << "[";
		for (size_t i = 0; i < tags.size(); ++i) {
			oss << tags[i];
			if (i + 1 < tags.size()) oss << ", ";
		}
		oss << "]";
		return oss.str();
	}


}