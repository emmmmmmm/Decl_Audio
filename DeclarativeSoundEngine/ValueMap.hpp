#pragma once
#include <unordered_map>
#include <string>

class ValueMap {
public:
	void SetValue(const std::string& key, float value);
	bool HasValue(const std::string& key) const;
	float GetValue(const std::string& key, float defaultValue = 0.0f) const;
	std::vector<std::pair<std::string, float>> GetAllValues() const;

private:
	std::unordered_map<std::string, float> values;
};