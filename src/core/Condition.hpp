// Condition.hpp
#pragma once
#include <string>
#include <unordered_map>
#include "ValueMap.hpp"

struct Condition {
    std::string text;

    Condition() = default;
    explicit Condition(std::string s) : text(std::move(s)) {}
    float eval(const ValueMap& entityVals, const ValueMap& globalVals) const;
};

