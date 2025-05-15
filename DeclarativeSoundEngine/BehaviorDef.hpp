#pragma once
#include <string>
#include "Condition.hpp"
#include "Expression.hpp"
#include "Node.hpp"
#include <memory>
#include <unordered_map>
#include <regex>
#include "Vec3.hpp"


struct BehaviorDef {

    uint32_t id = {};
    std::string name = {};
    std::vector<std::string> matchTags;
    std::vector<Condition>   matchConditions;
    std::unordered_map<std::string, Expression> parameters;

    std::shared_ptr<Node> onStart;   
    std::shared_ptr<Node> onActive;  
    std::shared_ptr<Node> onEnd;     

    int busIndex = {};
    Expression rootVolume = {};
};