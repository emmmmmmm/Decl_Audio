#pragma once

#include "declsound_export.hpp"
#include "AudioBehavior.hpp"
class SoundManager;

extern "C" {
    DECLSOUND_API SoundManager* CreateSoundManager();
    DECLSOUND_API void DestroySoundManager(SoundManager* mgr);
    DECLSOUND_API void SoundManager_Update(SoundManager* mgr);

    DECLSOUND_API void SoundManager_AddBehavior(SoundManager* mgr, const AudioBehavior* behavior);
    DECLSOUND_API void SoundManager_SetTag(SoundManager* mgr, const char* tag);

}