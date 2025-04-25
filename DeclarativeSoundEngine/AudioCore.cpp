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
    // this function will loop indefinitely in the future! 
    
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
    // we actually need to turn the behavior into an audio event! 
    // load audio data, parse what the soundbehavior actually does, etc!

}

void AudioCore::HandleStopBehavior(const Command& cmd) {
    LogMessage("AudioCore: StopBehavior called for " + cmd.soundName, LogCategory::AudioCore, LogLevel::Info);
    // todo: stop sound, cleanup from activebehaviors
}


void AudioCore::HandleValueUpdate(const Command& cmd) {
    LogMessage("AudioCore: ValueUpdate " + cmd.key + " = " + std::to_string(cmd.value), LogCategory::AudioCore, LogLevel::Info);
    // TODO: actually update values for behaviors
}

void AudioCore::ProcessActiveSounds() {
    // In the future, this will handle sound playback, fading, etc.
    // sounds that have ended will need to clean themselfs up from activeBehaviors as well

}
