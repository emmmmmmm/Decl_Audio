#include <iostream>
#include "SoundManagerAPI.hpp"
#include "Log.hpp"

int main() {

    LogMessage("Starting test...", LogCategory::CLI, LogLevel::Debug);
    SoundManager* mgr = CreateSoundManager();

    AudioBehavior behavior;
    
    LogMessage("Load Behaviors.", LogCategory::CLI, LogLevel::Debug);
    SoundManager_LoadBehaviorsFromFile(mgr, "test.audio");

    LogMessage("Set Tags.", LogCategory::CLI, LogLevel::Debug);
    SoundManager_SetTag(mgr, "player", "entity.player");
    SoundManager_SetTag(mgr, "player", "foot.leftContact");
    
    SoundManager_SetTag(mgr, "monster", "entity.monster");
    SoundManager_SetTag(mgr, "monster", "foot.leftContact");

    SoundManager_SetValue(mgr, "player", "velocity", 3.2f);
    SoundManager_SetValue(mgr, "player", "fatigue", 0.8f);
    
    SoundManager_SetGlobalValue(mgr, "time", 0.5f);

    LogMessage("Update.\n", LogCategory::CLI, LogLevel::Debug);

    SoundManager_Update(mgr);

    SoundManager_DebugPrintState(mgr);
    LogMessage("Clear Tags.", LogCategory::CLI, LogLevel::Debug);

    SoundManager_ClearTag(mgr, "player", "foot.leftContact");
    SoundManager_ClearTag(mgr, "player", "entity.player");
    SoundManager_ClearValue(mgr, "player", "velocity");

    SoundManager_ClearEntity(mgr,"monster");

    SoundManager_DebugPrintState(mgr);

    DestroySoundManager(mgr);
    LogMessage("Done.", LogCategory::CLI, LogLevel::Debug);

    return 0;
}
