#pragma once

#include "AudioBuffer.hpp"
#include "Vec3.hpp"

namespace Snapshot {
	constexpr int  kMaxVoices = 256;                 // worst-case active voices
	constexpr int  kMaxBuses = 16;                  // master + sub-buses
	constexpr int  kSnapCount = 3;                   // triple-buffer


	struct VoiceSnap {
		const AudioBuffer* buf{};
		size_t				playhead{};
		float				gain{};
		bool				loop{};
		uint8_t				bus{};
		uint64_t			startSample = {};
		std::vector<float>	pan = {}; // Spatialization
	};



	/* -------- immutable snapshot -------- */
	struct Snapshot {
		Vec3				listenerPosition = {};
		uint32_t			voiceCount = 0;
		VoiceSnap			voices[kMaxVoices];
		uint32_t			busCount = 1;                  // at least master
		float				busGain[kMaxBuses]{};
		std::vector<int>	busParent{};

		mutable std::vector<float> bus[kMaxBuses];  // resised once in ctor
	};
}