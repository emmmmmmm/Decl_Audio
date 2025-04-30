// MatchUtils.hpp
#pragma once
#include <string>

// we might actually want one for only perfect matches as well...?
inline bool PatternMatch(const std::string& pat, const std::string& val)
{
    size_t star = pat.find('*');
    if (star == std::string::npos)          // no wildcard → exact
        return pat == val;

    const std::string pre = pat.substr(0, star);
    const std::string post = pat.substr(star + 1);

    if (val.size() < pre.size() + post.size()) return false;
    if (!val.starts_with(pre))               return false;
    if (!val.ends_with(post))                return false;
    return true;
}