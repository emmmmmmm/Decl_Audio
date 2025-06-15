
#pragma once
#include <string>
#include <vector>
#include "Vec3.hpp"
#include "Quaternion.h"

#include <numeric>

using Quat = quaternion::Quaternion<float>;

struct Speaker {
    std::string name;         // e.g., "FL", "FR", "C", "LFE", "SL", "SR"
    Vec3 direction;           // Unit vector in listener space (e.g., FL = {-1, 0, 1}.normalized())
    float distance = 1.0f;    // Optional: how far speaker is from center
};

struct SpeakerLayout {
    std::vector<Speaker> speakers;


    static SpeakerLayout Stereo() {
        auto layout = SpeakerLayout();
        layout.speakers.push_back(Speaker{ "FL",{-1,0,1}, 1 });
        layout.speakers.push_back(Speaker{ "FR",{ 1,0,1}, 1 });
        return layout;
    }

    static SpeakerLayout FivePointOne() {
        SpeakerLayout layout;
        layout.speakers = {
            { "FL", Vec3{ -1.f, 0.f, 1.f }.Normalized() },
            { "FR", Vec3{  1.f, 0.f, 1.f }.Normalized() },
            { "C",  Vec3{  0.f, 0.f, 1.f } },
            { "LFE", Vec3{ 0.f, 0.f, 0.f } }, // sub is non-directional
            { "SL", Vec3{ -1.f, 0.f, 0.f } },
            { "SR", Vec3{  1.f, 0.f, 0.f } }
        };
        return layout;
    }
};



std::vector<float> ComputePanMask(
    const Vec3& sourcePosWorld,
    const Vec3& listenerPosWorld,
    const Quat& listenerRotation,
    const SpeakerLayout& layout
) {
    Vec3 dirWorld = Vec3::subtract(sourcePosWorld, listenerPosWorld).Normalized();
    Quat qInv = quaternion::Conjugate(listenerRotation);
    auto dirLocal = qInv.rotate(dirWorld);
    std::vector<float> out;
    for (const auto& speaker : layout.speakers) {
        float weight = (std::max)(0.f, Vec3::Dot(dirLocal, speaker.direction)); // cosine panning
        out.push_back(weight);
    }

    // optional normalization
    float sum = std::accumulate(out.begin(), out.end(), 0.f);
    if (sum > 0.f) for (auto& v : out) v /= sum;

    return out;
}