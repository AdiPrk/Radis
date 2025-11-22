/*****************************************************************//**
 * \file   Animation.hpp
 * \author Adi (aditya.prakash@digipen.edu)
 * \date   November 6 2024
 * \Copyright @ 2024 Digipen (USA) Corporation * 
 
 * \brief  Sets up the アニメーション data
 *  *********************************************************************/

#pragma once

#include "Graphics/Common/Model.h"
#include "Bone.h"
#include "Skeleton.h"

namespace Dog
{
    // This struct is used when creating the animation data
    struct AnimationNode
    {
        std::string debugName;
        VQS transformation;
        int id;
        int parentId;
        std::vector<int> childIndices;
        
        bool hasAnimatedDescendant = false;   // true if this node or any descendant maps to a Bone
        bool isIntermediate = false;          // true if debugName contains "$AssimpFbx$" (checked once)
        bool skipTransform = false;           // runtime flag: if true, use identity instead of node.transformation
    };

    class Animation
    {
    public:
        Animation();
        Animation(const aiScene* animScene, Model* model);
        ~Animation() {}

        Bone* FindBone(int id);

        float GetTicksPerSecond() const { return mTicksPerSecond; }
        float GetDuration() const { return mDuration; }
        const auto& GetBoneIDMap() { return mBoneInfoMap; }
        const auto& GetBoneMap() { return mBoneMap; }

        const std::vector<AnimationNode>& GetNodes() const { return mNodes; }
        const AnimationNode& GetNode(int index) const { return mNodes[index]; }
        int GetRootNodeIndex() const { return mRootNodeIndex; }

        // AI_SBBC_DEFAULT_MAX_BONES is defined in pch.h so that assimp builds correctly
        static const uint32_t MAX_BONES = AI_SBBC_DEFAULT_MAX_BONES;

    private:
        void ReadMissingBones(const aiAnimation* animation, Model& model);
        void ReadHeirarchyData(int parentIndex, const aiNode* src);
        void CheckNodesToSkip();

        float mDuration;
        float mTicksPerSecond;

        std::unordered_map<int, Bone> mBoneMap;
        std::unordered_map<int, BoneInfo> mBoneInfoMap;
        std::unordered_map<std::string, int> mNameToIDMap;

        Skeleton mSkeleton;
        std::vector<AnimationNode> mNodes;
        int mRootNodeIndex;
    };

}
