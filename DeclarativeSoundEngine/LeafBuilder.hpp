// LeafBuilder.hpp
#pragma once

#include <vector>
#include "Node.hpp"
#include "ValueMap.hpp"
#include "AudioBuffer.hpp"
#include "SoundManagerAPI.hpp" 
#include "AudioBufferManager.hpp"
#include "Expression.hpp"
#include "ValueMap.hpp"

namespace LeafBuilder {

    // Represents a playback instruction for AudioCore
    struct Leaf {
        const SoundNode*    src;     // unique identity in graph
        const AudioBuffer*  buffer;   // null for delay
        uint64_t            startSample;       // when (in samples) to begin
        uint64_t            durationSamples;   // for delay or sound length
        //float               volume;            // evaluated volume
        //float               pitch;             // evaluated pitch
        bool                loop;              // should this leaf loop
        int                 bus;               // output bus
        std::vector<Expression> volExprs{};
        std::vector<Expression> pitchExprs{};

        float volume(const ValueMap& params)const {
            float finalVol = 1.f;
            for (auto& e : volExprs)
                finalVol *= e.eval(params);
            return finalVol;
        }
        float pitch(const ValueMap& params)const {
            float finalPitch = 1.f;
            for (auto& e : pitchExprs)
                finalPitch *= e.eval(params);
            return finalPitch;
        }

    };
   

    static void BuildLeaves(const Node* node,
        const ValueMap& params,
        uint64_t startSample,
        bool inheritedLoop,
        std::vector<Expression> inheritedVols,
        std::vector<Expression> inheritedPitches,
        int bus,
        std::vector<Leaf>& out,
        AudioConfig* audioDeviceCfg,
        AudioBufferManager* bufferManager);

    void BuildLeaves(const Node* node,
        const ValueMap& params,
        uint64_t startSample,
        bool inheritedLoop,
        int bus,
        std::vector<Leaf>& out,
        AudioConfig* audioDeviceCfg,
        AudioBufferManager* bufferManager);
} // namespace LeafBuilder
