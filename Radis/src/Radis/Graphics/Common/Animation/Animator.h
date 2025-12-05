/*****************************************************************//**
 * \file   Animator.hpp
 * \author Adi (aditya.prakash@digipen.edu)
 * \date   November 6 2024
 * \Copyright @ 2024 Digipen (USA) Corporation * 
 
 * \brief  Animates an animation!!!
 *  *********************************************************************/

#pragma once

#include "Animation.h"

namespace Radis
{
    class Animator
    {
    public:
        Animator(Animation* animation);

        void UpdateAnimation(float dt);
        void UpdateAnimationInstant(float time, bool inPlace, glm::mat4& tr);
        void SetAnimation(Animation* pAnimation);
        void CalculateBoneTransform(int nodeIndex, const VQS& parentTransform, bool inPlace, const glm::mat4& tr = glm::mat4(1.0));

        const std::vector<VQS>& GetFinalBoneVQS() const { return mFinalBoneVQS; }
        bool IsPlaying() const { return mIsPlaying; }
        float GetPrevTime() const { return mPrevTime; }

    private:
        std::vector<VQS> mFinalBoneVQS;

        Animation* mCurrentAnimation;
        float mCurrentTime;
        float mPrevTime;

        bool mIsPlaying = true;
    };

} // namespace Radis
