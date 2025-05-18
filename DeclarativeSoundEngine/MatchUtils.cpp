#include "pch.h"
#include "MatchUtils.hpp"
#include "TagMap.hpp"
#include "ValueMap.hpp"
#include "BehaviorDef.hpp"


namespace MatchUtils {
	// we might actually want one for only perfect matches as well...?
	bool PatternMatch(const std::string& pat, const std::string& val)
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

	int TagSpecificity(const std::string& tag) {
		std::istringstream stream(tag);
		std::string segment;
		int score = 0;
		while (std::getline(stream, segment, '.'))
			score += (segment == "*") ? 5 : 10;
		return score;
	}

	bool TagMatches(const std::string& pattern, const std::string& actual) {
		std::istringstream pat(pattern);
		std::istringstream act(actual);
		std::string pseg, aseg;

		while (std::getline(pat, pseg, '.')) {
			if (!std::getline(act, aseg, '.')) return false;
			if (pseg != "*" && pseg != aseg) return false;
		}

		return !std::getline(act, aseg, '.');
	}


	int MatchScore(const BehaviorDef& def, const TagMap& tags, const TagMap& globalTags, const ValueMap& values, const ValueMap& globalValues) {

		int score = 0;
		auto allTags = tags.GetAllTags();
		const auto& gt = globalTags.GetAllTags();

		allTags.insert(allTags.end(), gt.begin(), gt.end());

		for (const auto& required : def.matchTags) {
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



		for (const auto& condition : def.matchConditions) {
			if (!condition.eval(values, globalValues)) {
				return -1;
			}
		}

		return score;
	}
}