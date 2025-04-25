#include "pch.h"
#include "TagMap.hpp"

void TagMap::AddTag(const std::string& tag) {
	tags.insert(tag);
}

void TagMap::RemoveTag(const std::string& tag) {
	tags.erase(tag);
}

bool TagMap::HasTag(const std::string& tag) const {
	return tags.find(tag) != tags.end();
}

std::vector<std::string> TagMap::GetAllTags() const {
	return std::vector<std::string>(tags.begin(), tags.end());
}
