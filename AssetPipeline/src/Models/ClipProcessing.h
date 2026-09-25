#pragma once

#include "AnimationFormat.h"
#include "ImportedScene.h"

// A clip in the file's layout (see AnimationFormat.h), sampled uniformly.
struct ProcessedClip
{
    uint32_t                          skeletonHash = 0;
    uint32_t                          frameCount = 1;
    float                             duration = 0.0f;
    float                             sampleRate = 0.0f;
    int32_t                           rootJoint = -1;
    std::vector<AnimationFile::Track> tracks;
    std::vector<glm::vec4>            constants;
    std::vector<glm::vec4>            samples;      // track-major, frameCount per animated track
    std::vector<glm::vec4>            rootMotion;   // one per frame when rootJoint >= 0
};

// Samples `animation` onto a model's skeleton. Joints are matched to the file's nodes by name, so
// the animation can come from another file with the same skeleton. The first joint whose
// translation moves (the hips of a character) has its horizontal motion taken out as root motion.
std::expected<ProcessedClip, std::string> ProcessClip(const ImportedAnimations& source, const ImportedAnimation& animation, const ImportedSkeleton& skeleton);

// The hash a clip for this skeleton records, as AnimationFile::SkeletonHash computes it from the
// cooked joints.
uint32_t SkeletonHash(const ImportedSkeleton& skeleton);
