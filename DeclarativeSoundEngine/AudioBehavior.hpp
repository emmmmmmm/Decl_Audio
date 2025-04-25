#pragma once
#include <string>
#include <vector>

struct AudioBehavior {
    std::string id;
    std::vector<std::string> matchTags; // e.g. ["player", "foot.leftContact"]
    std::vector<std::string> matchConditions; // e.g. ["velocity > 0.1", "fatigue < 0.9"]
    std::string soundName = "debug_test.wav";
};
