#include <PCH/pch.h>
#include "Animator.h"
#include "Animation.h"

#include "ECS/Systems/InputSystem.h"
#include "ECS/Resources/DebugDrawResource.h"
#include "ECS/Components/Components.h"


namespace Dog
{

    Animator::Animator(Animation* animation)
    {
        mCurrentTime = mPrevTime = 0.0;
        mCurrentAnimation = animation;

        // resize the final bone matrices based on biggest bone ID in the animation
        int maxBoneID = 0;
        for (const auto& pair : mCurrentAnimation->GetBoneIDMap())
        {
            maxBoneID = std::max(maxBoneID, pair.second.id);
        }

        mFinalBoneVQS.resize(maxBoneID + 1);
    }

    void Animator::UpdateAnimation(float dt)
    {
        if (mCurrentAnimation)
        {
            mPrevTime = mCurrentTime;
            mCurrentTime += mCurrentAnimation->GetTicksPerSecond() * dt;
            mCurrentTime = fmod(mCurrentTime, mCurrentAnimation->GetDuration());
  
            CalculateBoneTransform(mCurrentAnimation->GetRootNodeIndex(), VQS(), false);
        }
    }

    void Animator::UpdateAnimationInstant(float time, bool inPlace, glm::mat4& tr)
    {
        if (mCurrentAnimation)
        {
            mPrevTime = mCurrentTime;
            mCurrentTime = time;

            CalculateBoneTransform(mCurrentAnimation->GetRootNodeIndex(), VQS(), inPlace, tr);
        }
    }

    void Animator::SetAnimation(Animation* pAnimation)
    {
        mCurrentAnimation = pAnimation;
        mCurrentTime = 0.0f;
        mPrevTime = 0.0f;
    }

    void Animator::CalculateBoneTransform(int nodeIndex, const VQS& parentTransform, bool inPlace, const glm::mat4& tr)
    {
        const AnimationNode& node = mCurrentAnimation->GetNode(nodeIndex);
        int nodeId = node.id;
        VQS nodeTransform;

        Bone* bone = mCurrentAnimation->FindBone(nodeId);
        if (bone)
        {
            // Directly animated bone: use keyframed transform.
            bone->Update(mCurrentTime);
            nodeTransform = bone->GetLocalTransform();

            // inPlace only works for mixamo animations for now
            if (inPlace && bone->debugName == "mixamorig:Hips")
            {
                nodeTransform.translation = glm::vec3(0.0f);
            }
        }
        else if (node.skipTransform) nodeTransform = VQS();
        else nodeTransform = node.transformation;
     
        VQS globalTransformation = parentTransform * nodeTransform;

        // draw debug stuffs
        if (InputSystem::isKeyDown(Key::H))
        {
            if (node.parentId > 0 && ((bone && bone->debugName != "mixamorig:Hips") || !bone))
            {
                glm::vec3 start = glm::vec3(tr * glm::vec4(parentTransform.translation, 1.f));
                glm::vec3 end = glm::vec3(tr * glm::vec4(globalTransformation.translation, 1.f));

                DebugDrawResource::DrawLine(start, end, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
                DebugDrawResource::DrawCube(end, glm::vec3(0.01f), glm::vec4(0.0f, 1.0f, 1.0f, 0.4f));
            }
        }
        else
        {
            if (node.parentId > 0 && ((bone && bone->debugName != "mixamorig:Hips")))
            {
                glm::vec3 start = glm::vec3(tr * glm::vec4(parentTransform.translation, 1.f));
                glm::vec3 end = glm::vec3(tr * glm::vec4(globalTransformation.translation, 1.f));

                DebugDrawResource::DrawLine(start, end, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
                DebugDrawResource::DrawCube(end, glm::vec3(0.01f), glm::vec4(0.0f, 1.0f, 1.0f, 0.4f));
            }
        }

        const auto& boneInfoMap = mCurrentAnimation->GetBoneIDMap();
        auto boneIt = boneInfoMap.find(nodeId);
        if (boneIt != boneInfoMap.end())
        {
            const BoneInfo& info = boneIt->second;
            mFinalBoneVQS[info.id] = globalTransformation * info.vqsOffset;
        }

        for (int childIndex : node.childIndices)
        {
            CalculateBoneTransform(childIndex, globalTransformation, inPlace, tr);
        }
    }

} // namespace Dog
