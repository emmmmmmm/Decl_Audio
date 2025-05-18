// voice.hpp
#pragma once
#include "AudioDevice.hpp"

struct Voice {
	SoundHandle			handle = {};
	const AudioBuffer* buffer = {};
	size_t				playhead = {};
	float				currentVol = {};
	float				targetVol = {};
	float				volStep = {};
	int					busIndex = {};
	uint64_t			startSample = {};

	bool loop = false;
	const SoundNode* source = nullptr;


	// TBD, for pitching/resampling
	float currentPitch = {};
	float targetPitch = {};


	// concept from ObjectFactory
	void reset() {
		// e.g. reset playhead, clear buffers, zero flags…
		playhead = {};
		buffer = {};
		// etc.
	}

	bool Finished() {
		if (!buffer)return true; //!??
		return !loop && playhead >= buffer->GetFrameCount();
	}
};