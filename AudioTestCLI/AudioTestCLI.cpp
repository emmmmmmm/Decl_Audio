#include <iostream>
#include "SoundManagerAPI.hpp"
#include "AudioBehavior.hpp"
#include <BehaviorLoader.hpp>


int main() {
    std::cout << "[CLI] Starting test..." << std::endl;

    SoundManager* mgr = CreateSoundManager();

    AudioBehavior behavior;
    std::cout << "[CLI] Load Behaviors." << std::endl;
    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");

    std::cout << "[CLI] Set Tags." << std::endl;
    SoundManager_SetTag(mgr, "player", "entity.player");
    SoundManager_SetTag(mgr, "player", "foot.leftContact");
    
    SoundManager_SetTag(mgr, "monster", "entity.monster");
    SoundManager_SetTag(mgr, "monster", "foot.leftContact");

    SoundManager_SetValue(mgr, "player", "velocity", 3.2f);
    SoundManager_SetValue(mgr, "player", "fatigue", 0.8f);
    
    SoundManager_SetGlobalValue(mgr, "time", 0.5f);
    std::cout << "[CLI] Update.\n" << std::endl;

    SoundManager_Update(mgr);

    std::cout <<  std::endl;
    SoundManager_DebugPrintState(mgr);
    std::cout << "\n[CLI] Clear Tag." << std::endl;

    SoundManager_ClearTag(mgr, "player", "foot.leftContact");
    SoundManager_ClearTag(mgr, "player", "entity.player");
    SoundManager_ClearValue(mgr, "player", "velocity");

    SoundManager_ClearEntity(mgr,"monster");

    SoundManager_DebugPrintState(mgr);

    DestroySoundManager(mgr);
    std::cout << "[CLI] Done." << std::endl;
    return 0;
}
