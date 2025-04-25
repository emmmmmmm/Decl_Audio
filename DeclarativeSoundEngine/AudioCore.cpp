#include "pch.h"
#include "AudioCore.hpp"
#include "Log.hpp"

AudioCore::AudioCore(SoundManager* mgr, CommandQueue* fromManager, CommandQueue* toManager)
    : mgr(mgr), inQueue(fromManager), outQueue(toManager)
{

    // DEBUG:
    /*
    Command testCommand;
    testCommand.type = CommandType::Log;
    testCommand.strValue = "DebugTestLogThingyFromCore";
    outQueue->push(testCommand);
    */
}
AudioCore::~AudioCore() {}

void AudioCore::Update() {
    ProcessCommands();
    ProcessActiveSounds();
}

void AudioCore::ProcessCommands() {
    while (!inQueue->empty()) {
        Command cmd = inQueue->front();
        inQueue->pop();

        switch (cmd.type) {
        case CommandType::StartBehavior:
            HandleStartBehavior(cmd);
            break;
        case CommandType::StopBehavior:
            HandleStopBehavior(cmd);
            break;
        case CommandType::ValueUpdate:
            HandleValueUpdate(cmd);
            break;
        default:
            break;
        }
    }
}

void AudioCore::HandleStartBehavior(const Command& cmd) {
    LogMessage("AudioCore: StartBehavior called for " + cmd.soundName, LogCategory::AudioCore, LogLevel::Info);
    activeBehaviors.push_back(cmd.behavior);
}

void AudioCore::HandleStopBehavior(const Command& cmd) {
    LogMessage("AudioCore: StopBehavior called for " + cmd.soundName, LogCategory::AudioCore, LogLevel::Info);
}

void AudioCore::HandleValueUpdate(const Command& cmd) {
    LogMessage("AudioCore: ValueUpdate " + cmd.key + " = " + std::to_string(cmd.value), LogCategory::AudioCore, LogLevel::Info);
}

void AudioCore::ProcessActiveSounds() {
    // In the future, this will handle sound playback, fading, etc.
}
