#include <PCH/pch.h>
#include "AnimationLibrary.h"
#include "Animator.h"

namespace Radis
{
    const uint32_t AnimationLibrary::INVALID_ANIMATION_INDEX = 10001;

    AnimationLibrary::AnimationLibrary()
    {
    }

    AnimationLibrary::~AnimationLibrary()
    {
    }

    uint32_t AnimationLibrary::AddAnimation(const std::string& animPath, Model* model)
    {
        if (!model)
        {
            DOG_WARN("Model is null, cannot add animation from path: {0}", animPath);
            return INVALID_ANIMATION_INDEX;
        }

        //std::string animName = std::filesystem::path(animPath).stem().string();

        std::string key = GetKey(model->GetName(), animPath);
        if (mAnimationMap.find(key) != mAnimationMap.end())
        {
            return GetAnimationIndex(model->GetName(), animPath);
        }

        static Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(animPath, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_GlobalScale | aiProcess_OptimizeGraph);

        if (!scene || !scene->mAnimations[0] || !scene->mRootNode)
        {
            return INVALID_ANIMATION_INDEX;
        }

        uint32_t animationID = static_cast<uint32_t>(mAnimation.size());
        
        mAnimation.emplace_back(std::make_unique<Animation>(scene, model));
        mAnimators.emplace_back(std::make_unique<Animator>(mAnimation.back().get()));

        mAnimationMap[key] = animationID;

        return animationID;
    }

    Animation* AnimationLibrary::GetAnimation(const std::string& modelPath, const std::string& animPath)
    {
        std::string key = GetKey(modelPath, animPath);
        if (mAnimationMap.find(key) == mAnimationMap.end())
        {
            return nullptr;
        }
        uint32_t index = mAnimationMap[key];
        return GetAnimation(index);
    }

    Animation* AnimationLibrary::GetAnimation(uint32_t index)
    {
        if (index >= mAnimation.size())
        {
            return nullptr;
        }

        return mAnimation[index].get();
    }

    Animator* AnimationLibrary::GetAnimator(uint32_t index)
    {
        if (index >= mAnimators.size())
        {
            //DOG_WARN("Animator ID {0} is out of range!", index);
            return nullptr;
        }
        return mAnimators[index].get();
    }

    uint32_t AnimationLibrary::GetAnimationIndex(const std::string& modelPath, const std::string& animPath)
    {
        std::string key = GetKey(modelPath, animPath);
        if (mAnimationMap.find(key) == mAnimationMap.end())
        {
            return INVALID_ANIMATION_INDEX;
        }
        return mAnimationMap[key];
    }

    const std::string& AnimationLibrary::GetAnimationName(uint32_t index) const
    {
        if (index >= mAnimation.size())
        {
            static std::string empty = "";
            return empty;
        }

        // Reverse lookup in the map (inefficient but infrequent operation)
        for (const auto& pair : mAnimationMap)
        {
            if (pair.second == index)
            {
                return pair.first;
            }
        }

        // Should not reach here
        static std::string empty = "";
        return empty; 
    }

    const std::vector<VQS>& AnimationLibrary::GetAnimationVQS()
    {
        mAnimationVQS.clear();

        for (const auto& animator : mAnimators)
        {
            const auto& finalVQS = animator->GetFinalBoneVQS();
            mAnimationVQS.insert(mAnimationVQS.end(), finalVQS.begin(), finalVQS.end());
        }

        return mAnimationVQS;
    }

    void AnimationLibrary::UpdateAnimations(float dt)
    {
        for (const auto& animator : mAnimators)
        {
            if (!animator->IsPlaying()) continue;
            animator->UpdateAnimation(dt);
        }
    }


    void AnimationLibrary::UpdateAnimation(uint32_t index, float dt)
    {
        if (index >= mAnimators.size())
        {
            return;
        }

        mAnimators[index]->UpdateAnimation(dt);
    }

    std::string AnimationLibrary::GetKey(const std::string& modelPath, const std::string& animPath)
    {
        std::string key = modelPath + "|" + animPath;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        return key;
    }
}
