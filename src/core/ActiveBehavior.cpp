#include "pch.h"
#include "ActiveBehavior.hpp"
#include "LeafBuilder.hpp"



bool ActiveBehavior::HasVoice(const SoundNode* src) const
{
	return std::any_of(voices.begin(), voices.end(),
		[&](const Voice& v) { return v.source == src; });
}

Voice* ActiveBehavior::FindVoiceForLeaf(const LeafBuilder::Leaf& leaf) {
	for (auto& v : voices) {
		if (v.buffer == leaf.buffer
			&& v.source == leaf.src
			// this should be sufficient, but could add more fields like busIndex, pitch, etc.
			) {
			return &v;
		}
	}
	return nullptr;
}

Voice* ActiveBehavior::AddVoice(Voice&& v) {
	voices.push_back(std::move(v));
	return &voices.back();
}

void ActiveBehavior::StopAllVoices()
{
	voices.clear();
}

void ActiveBehavior::RemoveFinishedVoices()
{
	voices.erase(
		std::remove_if(
			voices.begin(),
			voices.end(),
			[]( Voice& v) { return v.Finished(); }
		),
		voices.end()
	);
}

