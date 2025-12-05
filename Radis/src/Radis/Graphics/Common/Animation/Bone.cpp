#include <PCH/pch.h>
#include "Bone.h"
#include <concepts>

/*reads keyframes from aiNodeAnim*/
namespace Dog
{
    // default ctor required for some stl containers
    Bone::Bone()
        : mID(-1)
        , mLocalTransform()
    {
    }

    Bone::Bone(int ID)
        : mID(ID)
        , mLocalTransform()
    {
    }

    Bone::Bone(int ID, const aiNodeAnim* channel, const std::string& debugName)
        : mID(ID)
        , mLocalTransform()
        , debugName(debugName)
    {
        for (unsigned ind = 0; ind < channel->mNumPositionKeys; ++ind)
        {
            auto& key = channel->mPositionKeys[ind];
            mPositions.emplace_back(aiVecToGlm(key.mValue), static_cast<float>(key.mTime));
        }

        for (unsigned ind = 0; ind < channel->mNumRotationKeys; ++ind)
        {
            auto& key = channel->mRotationKeys[ind];
            mRotations.emplace_back(aiQuatToGlm(key.mValue), static_cast<float>(key.mTime));
        }

        for (unsigned ind = 0; ind < channel->mNumScalingKeys; ++ind)
        {
            auto& key = channel->mScalingKeys[ind];
            mScales.emplace_back(aiVecToGlm(key.mValue), static_cast<float>(key.mTime));
        }
    }

    void Bone::Update(float animationTime)
    {
        if (!mPositions.empty()) InterpolatePosition(animationTime);
        if (!mRotations.empty()) InterpolateRotation(animationTime);
        if (!mScales.empty())    InterpolateScaling(animationTime);
    }

    template<typename T>
    concept Keyframe = requires(const T& k) {
        { k.time } -> std::convertible_to<float>; 
    };

    template<Keyframe KeyframeType>
    constexpr int FindKeyframeIndex(float animationTime, std::span<const KeyframeType> keyframes)
    {
        if (keyframes.size() <= 1) 
        {
            return 0;
        }

        const auto it = std::ranges::lower_bound(keyframes, animationTime, {}, &KeyframeType::time);
        const int index = static_cast<int>(it - keyframes.begin());

        return (index > 0) ? (index - 1) : 0;
    }

    float Bone::GetScaleFactor(float lastTime, float nextTime, float animationTime) const
    {
        float midWayLength = animationTime - lastTime;
        float framesDiff = nextTime - lastTime;
        if (framesDiff == 0.0f) return 0.0f;
        return midWayLength / framesDiff;
    }

    void Bone::InterpolatePosition(float animationTime)
    {
        if (mPositions.size() == 1)
        {
            mLocalTransform.translation = mPositions[0].position;
            return;
        }

        int p0 = FindKeyframeIndex<KeyPosition>(animationTime, mPositions);
        int p1 = (p0 + 1) % mPositions.size();

        float scaleFactor = GetScaleFactor(mPositions[p0].time, mPositions[p1].time, animationTime);
        mLocalTransform.translation = glm::mix(mPositions[p0].position, mPositions[p1].position, scaleFactor);
    }

    void Bone::InterpolateRotation(float animationTime)
    {
        if (mRotations.size() == 1)
        {
            mLocalTransform.rotation = glm::normalize(mRotations[0].orientation);
            return;
        }

        int p0 = FindKeyframeIndex<KeyRotation>(animationTime, mRotations);
        int p1 = (p0 + 1) % mRotations.size();

        float scaleFactor = GetScaleFactor(mRotations[p0].time, mRotations[p1].time, animationTime);

        mLocalTransform.rotation = glm::slerp(mRotations[p0].orientation, mRotations[p1].orientation, scaleFactor);
        mLocalTransform.rotation = glm::normalize(mLocalTransform.rotation); // Normalize after slerp
    }

    void Bone::InterpolateScaling(float animationTime)
    {
        if (mScales.size() == 1)
        {
            mLocalTransform.scale = mScales[0].scale;
            return;
        }

        int p0 = FindKeyframeIndex<KeyScale>(animationTime, mScales);
        int p1 = (p0 + 1) % mScales.size();

        float scaleFactor = GetScaleFactor(mScales[p0].time, mScales[p1].time, animationTime);
        mLocalTransform.scale = glm::mix(mScales[p0].scale, mScales[p1].scale, scaleFactor);
    }
}