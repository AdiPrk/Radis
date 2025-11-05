#pragma once

#include "../Vulkan/VKMesh.h"
#include "../Common/Animation/Bone.h"
#include "../Common/Animation/Skeleton.h"

namespace Dog
{
    class Device;

    class Model
    {
    public:
        //Remove copy constructor/operation from class 
        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        Model(Device& device, const std::string& filePath);
        ~Model();

        std::vector<std::unique_ptr<IMesh>> mMeshes;

        std::unordered_map<std::string, BoneInfo>& GetBoneInfoMap() { return mSkeleton.GetBoneInfoMap(); }
        const std::unordered_map<std::string, BoneInfo>& GetBoneInfoMap() const { return mSkeleton.GetBoneInfoMap(); }
        int& GetBoneCount() { return mSkeleton.GetBoneCount(); }

        glm::vec3 GetModelCenter() const { return glm::vec3(mAnimationTransform.x, mAnimationTransform.y, mAnimationTransform.z); }
        float GetModelScale() const { return mAnimationTransform.w; }

        const std::string& GetName() const { return mModelName; }
        const std::string& GetDir() const { return mDirectory; }

        const glm::mat4& GetNormalizationMatrix() const { return mNormalizationMatrix; }

        Assimp::Importer importer;
        const aiScene* mScene = nullptr;
        Skeleton mSkeleton;

    private:
        // Create vertex and index buffers for all meshes
        void CreateMeshBuffers(Device& device);

        // Load and process model using assimp
        void LoadMeshes(const std::string& filepath);

        void ProcessNode(aiNode* node, const glm::mat4& parentTransform = glm::mat4(1.f));
        IMesh& ProcessMesh(aiMesh* mesh, const glm::mat4& transform);

        void ProcessMaterials(aiMesh* mesh, IMesh& newMesh);
        void ProcessBaseColor(const aiScene* scene, aiMaterial* material, IMesh& newMesh);
        void ProcessDiffuseTexture(const aiScene* scene, aiMaterial* material, IMesh& newMesh);

        void NormalizeModel();

        void ExtractBoneWeights(std::vector<Vertex>& vertices, aiMesh* mesh);

        glm::vec3 mAABBmin;
        glm::vec3 mAABBmax;

        friend class ModelLibrary;
        bool mAddedTexture = false;
        std::string mModelName;
        std::string mDirectory; // For texture loading

        glm::mat4 mNormalizationMatrix;

        // Animation data
        glm::vec4 mAnimationTransform = glm::vec4(0.f); // xyz = center, w = inv scale
    };
}