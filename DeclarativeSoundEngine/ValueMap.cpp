// ValueMap.cpp
#include "pch.h"
#include "ValueMap.hpp"
#include <iostream>


// internal template to reduce repetition
template<typename T>
bool getHelper(const std::unordered_map<std::string, Value>& m,
	const std::string& key, T& out)
{
	auto it = m.find(key);
	if (it == m.end()) return false;
	if (auto ptr = std::get_if<T>(&it->second)) {
		out = *ptr;
		return true;
	}
	return false;




}

void ValueMap::SetValue(const std::string& key, float value) {
	values[key] = value;
}

void ValueMap::SetValue(const std::string& key, const std::string& value)
{
	values[key] = value;
}

void ValueMap::SetValue(const std::string& key, const Vec3& value)
{
	//values[key] = Value{ value }; // TODO
	values.insert_or_assign(key, Value{ value });
}

void ValueMap::SetValue(const std::string& key, const Value& value)
{
	values.insert_or_assign(key, value);
}

bool ValueMap::HasValue(const std::string& key) const {
	return values.find(key) != values.end();
}

bool ValueMap::TryGetValue(const std::string& key, float& out) const
{
	return getHelper(values, key, out);
}

bool ValueMap::TryGetValue(const std::string& key, std::string& out) const
{
	return getHelper(values, key, out);
}

bool ValueMap::TryGetValue(const std::string& key, Vec3& out) const
{
	return getHelper(values, key, out);
}


/*
float ValueMap::GetValue(const std::string& key, float defaultValue) const {
	auto it = values.find(key);
	return it != values.end() ? it->second : defaultValue;
}
*/
void ValueMap::ClearValue(const std::string& key) {
	values.erase(key);
}


std::unordered_map<std::string, Value> ValueMap::GetAllValues() const {
	return values;
}
