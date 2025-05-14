// MatchUtils.hpp
#pragma once
#include <string>
#include <sstream>
#include <regex>
#include "IBehaviorDefinition.hpp"
#include "TagMap.hpp"
#include "ValueMap.hpp"

namespace MatchUtils {
	// we might actually want one for only perfect matches as well...?
	inline bool PatternMatch(const std::string& pat, const std::string& val)
	{
		size_t star = pat.find('*');
		if (star == std::string::npos)          // no wildcard → exact
			return pat == val;

		const std::string pre = pat.substr(0, star);
		const std::string post = pat.substr(star + 1);

		if (val.size() < pre.size() + post.size()) return false;
		if (!val.starts_with(pre))               return false;
		if (!val.ends_with(post))                return false;
		return true;
	}


	static inline int TagSpecificity(const std::string& tag) {
		std::istringstream stream(tag);
		std::string segment;
		int score = 0;
		while (std::getline(stream, segment, '.'))
			score += (segment == "*") ? 5 : 10;
		return score;
	}

	static inline bool TagMatches(const std::string& pattern, const std::string& actual) {
		std::istringstream pat(pattern);
		std::istringstream act(actual);
		std::string pseg, aseg;

		while (std::getline(pat, pseg, '.')) {
			if (!std::getline(act, aseg, '.')) return false;
			if (pseg != "*" && pseg != aseg) return false;
		}

		return !std::getline(act, aseg, '.');
	}


	static inline int MatchScore(const MatchDefinition& md, const TagMap& entityMap, const TagMap& globalMap, std::unordered_map<std::string, ValueMap> entityValues, const std::string& entityId) {
		int score = 0;
		auto allTags = entityMap.GetAllTags();
		const auto& globalTags = globalMap.GetAllTags();
		allTags.insert(allTags.end(), globalTags.begin(), globalTags.end());

		for (const auto& required : md.matchTags) {
			bool matched = false;
			for (const auto& actual : allTags) {
				if (TagMatches(required, actual)) {
					matched = true;
					score += 10 + TagSpecificity(required);
					break;
				}
			}
			if (!matched) return -1;
		}

		static const ValueMap emptyVals;
		const ValueMap& entityVals = entityValues.count(entityId) ? entityValues.at(entityId) : emptyVals;
		const ValueMap& globalVals = entityValues.count("global") ? entityValues.at("global") : emptyVals;

		for (const auto& condition : md.matchConditions) {
			if (!condition.eval(entityVals, globalVals)) {
				return -1;
			}
		}

		return score;
	}
}