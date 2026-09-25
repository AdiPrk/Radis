/*****************************************************************//**
 * \file   AnimationLibrary.cpp
 * \brief  Implementation of the AnimationLibrary class for managing animation clips.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "AnimationLibrary.h"
#include "AnimationClip.h"

namespace Radis
{
    const uint32_t AnimationLibrary::INVALID_ANIMATION_INDEX = UINT32_MAX;

    AnimationLibrary::AnimationLibrary()
    {
    }

    AnimationLibrary::~AnimationLibrary()
    {
    }

    uint32_t AnimationLibrary::AddClip(const std::string& path)
    {
        if (const auto found = mClipIndices.find(path); found != mClipIndices.end())
        {
            return found->second;
        }

        std::unique_ptr<AnimationClip> clip = AnimationClip::Load(path);
        if (!clip)
        {
            RADIS_WARN("Failed to load animation clip {}", path);
            mClipIndices.emplace(path, INVALID_ANIMATION_INDEX);
            return INVALID_ANIMATION_INDEX;
        }

        const uint32_t index = static_cast<uint32_t>(mClips.size());
        mClips.push_back(std::move(clip));
        mClipIndices.emplace(path, index);
        return index;
    }

    const AnimationClip* AnimationLibrary::GetClip(uint32_t index) const
    {
        return index < mClips.size() ? mClips[index].get() : nullptr;
    }
}
