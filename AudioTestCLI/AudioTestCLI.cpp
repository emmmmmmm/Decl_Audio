#include <iostream>
#include "SoundManagerAPI.hpp"
#include "AudioBehavior.hpp"
#include <BehaviorLoader.hpp>


int main() {
    std::cout << "[CLI] Starting test..." << std::endl;

    SoundManager* mgr = CreateSoundManager();

    AudioBehavior behavior;
    behavior.id = "footstep.audio";
    behavior.matchTags = { "player", "foot.leftContact" };
    behavior.soundName = "debug_test.wav";

    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");

    SoundManager_SetTag(mgr, "player");
    SoundManager_SetTag(mgr, "foot.leftContact");

    SoundManager_SetValue(mgr, "velocity", 3.2f);
    SoundManager_SetValue(mgr, "fatigue", 0.8f);

    SoundManager_Update(mgr);
    SoundManager_DebugPrintState(mgr);

    SoundManager_ClearTag(mgr, "player");

    SoundManager_DebugPrintState(mgr);

    DestroySoundManager(mgr);
    std::cout << "[CLI] Done." << std::endl;
    return 0;
}
