#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

class TagMap {
public:
	void AddTag(const std::string& tag);
	void RemoveTag(const std::string& tag);
	bool HasTag(const std::string& tag) const;
	std::vector<std::string> GetAllTags() const;

private:
	std::unordered_set<std::string> tags;
};