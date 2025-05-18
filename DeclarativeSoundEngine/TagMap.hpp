// TagMap.hpp
#pragma once

#include <string>
#include <vector>
#include <unordered_set>

class TagMap {
public:

    void AddTag(const std::string& tag, bool transient = false);
    void RemoveTag(const std::string& tag);
    bool HasTag(const std::string& tag) const;
    std::vector<std::string> GetAllTags() const;
    void ClearTransient();

private:
    std::unordered_set<std::string> _persistent;
    std::unordered_set<std::string> _transient;
};