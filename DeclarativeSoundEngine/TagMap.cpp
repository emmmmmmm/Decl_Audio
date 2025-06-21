// TagMap.cpp
#include "pch.h"
#include "TagMap.hpp"
#include "Log.hpp"

void TagMap::AddTag(const std::string& tag, bool transient) {
    if (transient)
        _transient.insert(tag);
    else
        _persistent.insert(tag);
}

void TagMap::RemoveTag(const std::string& tag) {
    _persistent.erase(tag);
    _transient.erase(tag);
}

bool TagMap::HasTag(const std::string& tag) const {
    return _persistent.count(tag) || _transient.count(tag);
}

std::vector<std::string> TagMap::GetAllTags() const {
    std::vector<std::string> out;
    out.reserve(_persistent.size() + _transient.size());
    for (auto& t : _persistent) out.push_back(t);
    for (auto& t : _transient) out.push_back(t);
    return out;
}

void TagMap::ClearTransient() {
    if(_transient.size()>0)
        LogMessage("clearning " + std::to_string(_transient.size())
            + " transient tags", LogCategory::Entity, LogLevel::Info);

    _transient.clear();
}

std::vector<std::string> TagMap::GetTransientTags()
{
    std::vector<std::string> out;
    out.reserve(_transient.size());
    for (auto& t : _transient) out.push_back(t);
    return out;
}
