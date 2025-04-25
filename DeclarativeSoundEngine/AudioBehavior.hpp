#pragma once
#include <string>
#include <vector>

struct AudioBehavior {
    std::string id;
    std::vector<std::string> matchTags; // e.g. ["player", "foot.leftContact"]

    // Placeholder for now — transform data will go here later
    std::string soundName = "debug_test.wav";
};
