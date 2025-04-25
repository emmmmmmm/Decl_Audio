#include "pch.h"
#include "ValueMap.hpp"

void ValueMap::SetValue(const std::string& key, float value) {
    values[key] = value;
}

bool ValueMap::HasValue(const std::string& key) const {
    return values.find(key) != values.end();
}

float ValueMap::GetValue(const std::string& key, float defaultValue) const {
    auto it = values.find(key);
    return it != values.end() ? it->second : defaultValue;
}