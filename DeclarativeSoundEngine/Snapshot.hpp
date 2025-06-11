#pragma once

#include "AudioBuffer.hpp"
#include "Vec3.hpp"

namespace Snapshot {
	constexpr int  kMaxVoices = 256;
	constexpr int  kMaxBuses  = 256;
	constexpr int  kSnapCount = 3;
	constexpr int kMaxVoicesPerBus = 128; // upper bound, or choose smaller if you know max voices/bus

	struct VoiceSnap {
		const AudioBuffer* buf = nullptr;
		size_t				playhead = 0;
		float				gain = 1.0f;
		bool				loop = false;
		uint8_t				bus = 0;
		uint64_t			startSample = 0;
		float				pan[2] = {1.0f, 1.0f};  // Stereo fixed
	};

	struct Snapshot {
		Vec3				listenerPosition = {};
		uint32_t			voiceCount = 0;
		VoiceSnap			voices[kMaxVoices];         // contiguous, flat
		uint32_t			busCount = 1;
		float				busGain[kMaxBuses]{};
		int					busParent[kMaxBuses]{};     // flattened from vector
		mutable float		bus[kMaxBuses][4096]{};     // optionally fixed-size
		
		uint32_t			numVoicesInBus[kMaxBuses] = {};
		uint32_t			voicesByBus[kMaxBuses][kMaxVoicesPerBus] = {};

	};
}
