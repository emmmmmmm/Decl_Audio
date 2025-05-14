// LeafBuilder.hpp
#pragma once

#include <vector>
#include "Node.hpp"
#include "ValueMap.hpp"
#include "AudioBuffer.hpp"
#include "SoundManagerAPI.hpp" 
#include "AudioBufferManager.hpp"
namespace LeafBuilder {

    // Represents a playback instruction for AudioCore
    struct Leaf {
        const SoundNode*    src;     // unique identity in graph
        const AudioBuffer*  buffer;   // null for delay
        double              startSample;       // when (in samples) to begin
        double              durationSamples;   // for delay or sound length
        float               volume;            // evaluated volume
        float               pitch;             // evaluated pitch
        bool                loop;              // should this leaf loop
        int                 bus;               // output bus
    };

   
    // Recursively traverse a Node graph and flatten into leaves
    // startSample: current sample-offset within the behavior timeline
    // inheritedLoop: true if any ancestor LoopNode is active
    // bus: audio bus index to route to
    // params: runtime parameters for eval
    void BuildLeaves(const Node* node,
        const ValueMap& params,
        double startSample,
        bool inheritedLoop,
        int bus,
        std::vector<Leaf>& out,
        AudioConfig* audioDeviceCfg,
        AudioBufferManager* bufferManager);

} // namespace LeafBuilder
