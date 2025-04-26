// Expression.hpp
#pragma once
#include <string>
#include <unordered_map>
#include "ValueMap.hpp"

/// Wraps evaluation of numeric expressions (e.g. "velocity * 0.8 + 0.1")
struct Expression {
    std::string text;
    Expression() = default;                
    explicit Expression(std::string s) : text(std::move(s)) {}
    float eval(const ValueMap& params) const;
};
