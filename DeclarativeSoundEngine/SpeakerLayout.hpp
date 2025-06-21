
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
        layout.speakers.push_back(Speaker{ "FL",{-1,0,0}, 1 });
        layout.speakers.push_back(Speaker{ "FR",{ 1,0,0}, 1 });
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

inline std::vector<float> ComputePanMask(
    const Vec3& sourcePosWorld,
    const Vec3& listenerPosWorld,
    const Quat& listenerRotation,
    const SpeakerLayout& layout,
    float spread = 0.1f
) {
    Vec3 dirWorld = Vec3::subtract(sourcePosWorld, listenerPosWorld);
    Vec3 dirLocal = quaternion::Conjugate(listenerRotation).rotate(dirWorld);

    if (dirLocal.squareMagnitude() == 0.f)
        dirLocal = { 0.f, 0.f, 1.f }; // fallback forward

    dirLocal = dirLocal.Normalized();

    float sigma = 0.75f + spread * 1.25f;
    std::vector<float> weights;
    weights.reserve(layout.speakers.size());

    for (const auto& speaker : layout.speakers) {
        Vec3 spkDir = speaker.direction;
        if (spkDir.squareMagnitude() == 0.f)
            spkDir = { 0.f, 0.f, 1.f };
        spkDir = spkDir.Normalized();
       /* std::cout << "spkDir :" 
            << std::to_string(spkDir.x) << " : "
            << std::to_string(spkDir.y) << " : "
            << std::to_string(spkDir.z) 
            << "\n";*/

        float cosAngle = Vec3::Dot(dirLocal, spkDir);
        float angle = std::acos(std::clamp(cosAngle, -1.f, 1.f)); // [0, π]
        //std::cout << "angle to " << speaker.name << ": " << std::to_string(angle) << "\n";
        float gain = std::exp(-(angle * angle) / (2.f * sigma * sigma));
        weights.push_back(gain);
    }

    // normalize
    float sum = std::accumulate(weights.begin(), weights.end(), 0.f);
    if (sum > 0.f)
        for (auto& w : weights) w /= sum;

    return weights;
}