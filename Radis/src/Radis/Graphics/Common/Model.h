/*****************************************************************//**
 * \file   Model.h
 * \brief  Definition of the Model class for 3D model loading and processing.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "../Common/Animation/Bone.h"
#include "../RHI/Mesh.h"

namespace Radis
{
    class ModelSerializer;

    class Device;

    class Model
    {
    public:
        Model() = default;
        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        Model(Device& device, const std::string& filePath, bool fromDM = false, bool toDM = false);
        ~Model();

        std::vector<std::unique_ptr<Mesh>> mMeshes;

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
        Mesh& ProcessMesh(aiMesh* mesh, const glm::mat4& transform);

        // Checks for textures in order of types to try
        std::string ResolveTexturePath(aiMaterial* material, const std::vector<aiTextureType>& typesToTry, std::vector<unsigned char>& outEmbeddedData);
        void ProcessMaterials(aiMesh* mesh, Mesh& newMesh);
        void ProcessVertexColor(aiMaterial* material, Mesh& newMesh);
        void ProcessBaseColor(aiMaterial* material, Mesh& newMesh);
        void ProcessNormalMap(aiMaterial* material, Mesh& newMesh);
        void ProcessPBRMaps(aiMaterial* material, Mesh& newMesh);
        void ProcessEmissive(aiMaterial* material, Mesh& newMesh);

        void NormalizeModel();
        void ExtractBoneWeights(std::vector<Vertex>& vertices, aiMesh* mesh);

        friend class ModelSerializer;
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