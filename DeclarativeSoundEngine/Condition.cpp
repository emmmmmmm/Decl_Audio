#include "pch.h"
#include "Condition.hpp"
#include "Log.hpp"
#include <regex>
#include <iostream>
#include "ValueMap.hpp"

float Condition::eval( const ValueMap& entityVals, const ValueMap& globalVals) const {
    static std::regex expr(R"((\w+)\s*([<>=!]+)\s*([\d\.]+))");
    std::smatch match;

    if (std::regex_match(text, match, expr)) {
        std::string key = match[1];
        std::string op = match[2];
        float value = std::stof(match[3]);
        float actual = 0;


        if (entityVals.TryGetValue(key, actual)) {
           // all good
        }
        else if (globalVals.TryGetValue(key,actual)){
            // pretty much done
        }
        else {
        // uh oh, value could not be found
        }
        //float actual = entityVals.HasValue(key) ? entityVals.GetValue(key) : globalVals.GetValue(key);

        if (op == ">") return actual > value;
        if (op == ">=") return actual >= value;
        if (op == "<") return actual < value;
        if (op == "<=") return actual <= value;
        if (op == "==") return actual == value;
        if (op == "!=") return actual != value;
    }

    return false; // malformed or unknown operator
}