#include <iostream>
#include "SoundManagerAPI.hpp"
#include "AudioBehavior.hpp"
int main() {
    std::cout << "[CLI] Starting test..." << std::endl;

    SoundManager* mgr = CreateSoundManager();

    AudioBehavior behavior;
    behavior.id = "footstep.audio";
    behavior.matchTags = { "player", "foot.leftContact" };
    behavior.soundName = "debug_test.wav";
    SoundManager_AddBehavior(mgr, &behavior);

    SoundManager_SetTag(mgr, "player");
    SoundManager_SetTag(mgr, "foot.leftContact");

    SoundManager_Update(mgr);

    DestroySoundManager(mgr);
    std::cout << "[CLI] Done." << std::endl;
    return 0;
}
