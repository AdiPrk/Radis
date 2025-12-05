#include <PCH/pch.h>
#include "Animation.h"

namespace Radis
{
    Animation::Animation()
        : mDuration(0.0f)
        , mTicksPerSecond(30.f)
        , mRootNodeIndex(0)
        , mNodes()
    {
    }

    Animation::Animation(const aiScene* animScene, Model* model)
        : mDuration(0.0f)
        , mTicksPerSecond(30.f)
        , mRootNodeIndex(0)
        , mNodes()
    {
        // Load the first animation
        aiAnimation* animation = animScene->mAnimations[0];

        mDuration = static_cast<float>(animation->mDuration);
        if (animation->mTicksPerSecond != 0)
        {
            mTicksPerSecond = static_cast<float>(animation->mTicksPerSecond);
        }

        ReadHeirarchyData(-1, animScene->mRootNode);
        ReadMissingBones(animation, *model);       
        CheckNodesToSkip();

        // clear data that we don't need anymore
        mNameToIDMap.clear();
    }

    Bone* Animation::FindBone(int id)
    {
        auto iter = mBoneMap.find(id);
        if (iter == mBoneMap.end()) return nullptr;
        else return &iter->second;
    }

    void Animation::ReadHeirarchyData(int parentIndex, const aiNode* src)
    {
        std::string nodeName = src->mName.data;

        // Get or create a unique ID for this node based on its name.
        int nodeId;
        auto it = mNameToIDMap.find(nodeName);
        if (it == mNameToIDMap.end())
        {
            nodeId = static_cast<int>(mNameToIDMap.size());
            mNameToIDMap[nodeName] = nodeId;
        }
        else
        {
            nodeId = it->second;
        }

        // The index for the current node will be the current size of the vector.
        int currentIndex = static_cast<int>(mNodes.size());

        // Create the new node.
        AnimationNode node;
        node.id = nodeId;
        node.parentId = parentIndex; // Correctly sets -1 for the root.
        node.debugName = nodeName;
        node.transformation = aiMatToGlm(src->mTransformation);
        mNodes.push_back(node);

        // If this is not the root node, add this node's index to its parent's list of children.
        if (parentIndex != -1)
        {
            mNodes[parentIndex].childIndices.push_back(currentIndex);
        }

        // Recursively process all children.
        for (unsigned int i = 0; i < src->mNumChildren; i++)
        {
            // Pass the current node's index as the parent index for its children.
            ReadHeirarchyData(currentIndex, src->mChildren[i]);
        }
    }

    void Animation::CheckNodesToSkip()
    {
        for (auto& node : mNodes) {
            node.skipTransform = (node.debugName.find("$AssimpFbx$") != std::string::npos);
        }
    }

    void Animation::ReadMissingBones(const aiAnimation* animation, Model& model)
    {
        std::unordered_map<std::string, BoneInfo>& boneInfoMap = model.GetBoneInfoMap();
        int& boneCount = model.GetBoneCount();

        for (unsigned i = 0; i < animation->mNumChannels; i++)
        {
            aiNodeAnim* channel = animation->mChannels[i];
            const std::string& boneName = channel->mNodeName.data;

            if (mNameToIDMap.find(boneName) == mNameToIDMap.end())
            {
                RADIS_WARN("Animation::ReadMissingBones: Node name '{}' not found in hierarchy, but requested for keyframes", boneName);
            }

            int nodeId = mNameToIDMap[boneName];
            
            if (boneInfoMap.find(boneName) == boneInfoMap.end())
            {
                boneInfoMap[boneName].id = boneCount++;
            }
            
            mBoneMap.try_emplace(nodeId, boneInfoMap[boneName].id, channel, boneName);
        }

        // Map from node IDs to BoneInfo
        mBoneInfoMap.clear();
        for (const auto& pair : boneInfoMap)
        {
            const std::string& boneName = pair.first;
            int nodeId = mNameToIDMap[boneName];
            mBoneInfoMap[nodeId] = pair.second;
        }
    }

} // namespace Radis
