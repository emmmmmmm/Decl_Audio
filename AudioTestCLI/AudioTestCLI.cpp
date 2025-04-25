#include <iostream>
#include "SoundManagerAPI.hpp"
#include "AudioBehavior.hpp"
#include <BehaviorLoader.hpp>


int main() {
    std::cout << "[CLI] Starting test..." << std::endl;

    SoundManager* mgr = CreateSoundManager();

    AudioBehavior behavior;

    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");

    SoundManager_SetTag(mgr, "player", "player");
    SoundManager_SetTag(mgr, "player", "foot.leftContact");

    SoundManager_SetValue(mgr, "player", "velocity", 3.2f);
    SoundManager_SetValue(mgr, "player", "fatigue", 0.8f);
    SoundManager_SetGlobalValue(mgr, "time", 0.5f);
    SoundManager_Update(mgr);
    SoundManager_DebugPrintState(mgr);

    SoundManager_ClearTag(mgr, "player", "player");

    SoundManager_DebugPrintState(mgr);

    DestroySoundManager(mgr);
    std::cout << "[CLI] Done." << std::endl;
    return 0;
}
