#pragma once

#include "Bone.h"

namespace Dog
{

    // Simple holder for bone info + id assignment.
    // Keeps responsibility for bone bookkeeping in one place.
    struct Skeleton
    {
        // Map from bone name -> BoneInfo (contains id, offset matrix, etc.)
        std::unordered_map<std::string, BoneInfo> boneInfoMap;

        // Next id to assign for newly discovered bones.
        int boneCount = 0;

        Skeleton() = default;

        // Find existing bone id or create a new BoneInfo for 'name' with given offset matrix.
        // Returns the bone id.
        int GetOrCreateBoneID(const std::string& name, const glm::mat4& offsetMat)
        {
            auto it = boneInfoMap.find(name);
            if (it == boneInfoMap.end())
            {
                BoneInfo info(boneCount, offsetMat); // assumes BoneInfo has ctor (int id, glm::mat4 offset)
                boneInfoMap.emplace(name, info);
                return boneCount++;
            }
            return it->second.id;
        }

        // Accessors
        std::unordered_map<std::string, BoneInfo>& GetBoneInfoMap() { return boneInfoMap; }
        const std::unordered_map<std::string, BoneInfo>& GetBoneInfoMap() const { return boneInfoMap; }
        int& GetBoneCount() { return boneCount; }
        size_t Size() const { return boneInfoMap.size(); }

        void Clear()
        {
            boneInfoMap.clear();
            boneCount = 0;
        }
    };

} // namespace Dog
