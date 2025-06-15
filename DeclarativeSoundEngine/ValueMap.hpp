#pragma once
#include <unordered_map>
#include <string>
#include <variant>
#include "Vec3.hpp"
#include "Quaternion.h"


using Quat = quaternion::Quaternion<float>;
using Value = std::variant<float, std::string, Vec3, bool, Quat>;

class ValueMap {
public:
	void SetValue(const std::string& key, float value);
	void SetValue(const std::string& key, const std::string& value);
	void SetValue(const std::string& key, const Vec3& value);
	void SetValue(const std::string& key, const Quat& value);
	void SetValue(const std::string& key, const Value& value);
	void SetValue(const std::string& key, const bool value);
	bool HasValue(const std::string& key) const;

	bool TryGetValue(const std::string& key, float& out) const;
	bool TryGetValue(const std::string& key, std::string& out) const;
	bool TryGetValue(const std::string& key, Vec3& out) const;
	bool TryGetValue(const std::string& key, Quat& out) const;
	bool TryGetValue(const std::string& key, bool out) const;
	
	void ClearValue(const std::string& key);

	std::unordered_map<std::string, Value> GetAllValues() const;

	std::unordered_map<std::string, Value> values;

	
};