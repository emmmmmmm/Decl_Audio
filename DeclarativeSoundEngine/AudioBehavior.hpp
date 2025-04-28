#pragma once
#include <string>
#include <vector>
#include "Condition.hpp"
#include "Expression.hpp"
#include "Node.hpp"
#include <memory>
#include <unordered_map>
#include <regex>

/*
idea: soundbehaviors should have a "t" variable, that represents the current point in time of the playing behavior
this would allow to create things like volume : 0.5*t , so a sound that get's louder over time, etc*/

struct AudioBehavior {

    uint32_t id; // NOT SET
    std::string name;
    std::vector<std::string> matchTags;
    std::vector<Condition>   matchConditions;
    std::unordered_map<std::string, Expression> parameters;
    std::shared_ptr<Node>    rootSoundNode;
};