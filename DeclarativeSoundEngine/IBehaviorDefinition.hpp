#pragma once

#include <vector>
#include <string>
#include "Condition.hpp"
#include "Expression.hpp"


struct MatchDefinition {
	uint32_t id = {};
	std::string name = {};
	std::vector<std::string> matchTags;
	std::vector<Condition>   matchConditions;
	std::unordered_map<std::string, Expression> parameters;
};

struct PlayDefinition { 
	uint32_t id = {};

	std::unique_ptr<Node> onStart;
	std::unique_ptr<Node> onActive;
	std::unique_ptr<Node> onEnd;


	std::unordered_map<std::string, Expression> parameters;
};


struct IBehaviorDefinition {
	virtual std::vector<PlayDefinition>& GetPlayDefs() = 0;
	virtual std::vector<MatchDefinition>& GetMatchDefs() = 0;
};