// TagMap.hpp
#pragma once

#include <string>
#include <vector>
#include <unordered_set>

class TagMap {
public:
    // Add a tag; if transient==true it will auto-clear after one Update()
    void AddTag(const std::string& tag, bool transient = false);

    // Remove a tag (whether persistent or transient)
    void RemoveTag(const std::string& tag);

    // Check for either persistent or transient
    bool HasTag(const std::string& tag) const;

    // Returns all tags (persistent + transient)
    std::vector<std::string> GetAllTags() const;

    // Clear only the transient (one-shot) tags
    void ClearTransient();

private:
    std::unordered_set<std::string> _persistent;
    std::unordered_set<std::string> _transient;
};