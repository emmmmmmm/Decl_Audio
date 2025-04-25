#pragma once
#include "AudioCommand.hpp"
#include "AudioBehavior.hpp"


class SoundManager; // forward declaration
class AudioCore {
public:
    AudioCore(SoundManager* mgr, CommandQueue* fromManager, CommandQueue* toManager);
    ~AudioCore();
    void Update();
    void ProcessCommands();
    void HandleStartBehavior(const Command& cmd);
    void HandleStopBehavior(const Command& cmd);
    void HandleValueUpdate(const Command& cmd);

private:
    // Internal sound data and behavior management
    SoundManager* mgr;
    std::vector<AudioBehavior> activeBehaviors;
    CommandQueue* inQueue;
    CommandQueue* outQueue;

    void ProcessActiveSounds();
};