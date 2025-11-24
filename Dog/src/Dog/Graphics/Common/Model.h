#pragma once

#include "../Vulkan/VKMesh.h"
#include "../Common/Animation/Bone.h"

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

        std::unordered_map<std::string, BoneInfo>& GetBoneInfoMap() { return mBoneInfoMap; }
        const std::unordered_map<std::string, BoneInfo>& GetBoneInfoMap() const { return mBoneInfoMap; }
        int& GetBoneCount() { return mBoneCount; }

        const std::string& GetName() const { return mModelName; }
        const std::string& GetDir() const { return mDirectory; }

        const glm::mat4& GetNormalizationMatrix() const { return mNormalizationMatrix; }

        Assimp::Importer importer;
        const aiScene* mScene = nullptr;

    private:
        // Load and process model using assimp
        void LoadMeshes(const std::string& filepath);
        void ProcessNode(aiNode* node, const glm::mat4& parentTransform = glm::mat4(1.f));
        IMesh& ProcessMesh(aiMesh* mesh, const glm::mat4& transform);

        // Checks for textures in order of types to try
        std::string ResolveTexturePath(aiMaterial* material, const std::vector<aiTextureType>& typesToTry, std::vector<unsigned char>& outEmbeddedData);
        void ProcessMaterials(aiMesh* mesh, IMesh& newMesh);
        void ProcessVertexColor(aiMaterial* material, IMesh& newMesh);
        void ProcessBaseColor(aiMaterial* material, IMesh& newMesh);
        void ProcessNormalMap(aiMaterial* material, IMesh& newMesh);
        void ProcessPBRMaps(aiMaterial* material, IMesh& newMesh);
        void ProcessEmissive(aiMaterial* material, IMesh& newMesh);

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
        std::unordered_map<std::string, BoneInfo> mBoneInfoMap;
        int mBoneCount = 0;
    };
}