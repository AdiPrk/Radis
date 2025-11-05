#include <PCH/pch.h>
#include "Model.h"
#include "../Vulkan/Core/Buffer.h"
#include "../Vulkan/VKMesh.h"
#include "../OpenGL/GLMesh.h"

#include "Graphics/RHI/RHI.h"

#include "Engine.h"

namespace Dog
{
    Model::Model(Device& device, const std::string& filePath)
    {
        LoadMeshes(filePath);
        CreateMeshBuffers(device);
    }

    Model::~Model()
    {
    }

    void Model::CreateMeshBuffers(Device& device)
    {
        for (auto& mesh : mMeshes)
        {
            mesh->CreateVertexBuffers(&device);
            mesh->CreateIndexBuffers(&device);
        }
    }

    void Model::LoadMeshes(const std::string& filepath)
    {
        mScene = importer.ReadFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_GlobalScale | aiProcess_OptimizeGraph);

        // Check if the scene was loaded successfully
        if (!mScene || mScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !mScene->mRootNode)
        {
            DOG_CRITICAL("Assimp Error: {}", importer.GetErrorString());
            return;
        }

        // change all double backslash to forwardslash in filepath
        std::string newPath = filepath;
        std::replace(newPath.begin(), newPath.end(), '\\', '/');

        std::filesystem::path pathObj(newPath);
        mDirectory = pathObj.parent_path().string();
        mModelName = pathObj.stem().string();

        ProcessNode(mScene->mRootNode);

        // Scale all meshes to fit in a 1x1x1 cube and translate to 0,0,0
        NormalizeModel();
    }

    void Model::ProcessNode(aiNode* node, const glm::mat4& parentTransform)
    {
        // Convert the node's transformation matrix
        glm::mat4 nodeTransform = aiMatToGlm(node->mTransformation);

        // Combine this node's transformation with the parent transformation
        glm::mat4 globalTransform = parentTransform * nodeTransform;

        // Process each mesh in the current node
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* aMesh = mScene->mMeshes[node->mMeshes[i]];
            ProcessMesh(aMesh, globalTransform);
        }

        // Recursively process each child node
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(node->mChildren[i], globalTransform);
        }
    }

    IMesh& Model::ProcessMesh(aiMesh* mesh, const glm::mat4& transform)
    {
        std::unique_ptr<IMesh>* newMeshPtrRaw = nullptr;

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            newMeshPtrRaw = &mMeshes.emplace_back(std::make_unique<VKMesh>());
        }
        else
        {
            newMeshPtrRaw = &mMeshes.emplace_back(std::make_unique<GLMesh>());
        }

        IMesh& newMesh = **newMeshPtrRaw;

        glm::vec3 meshMin(std::numeric_limits<float>::max());
        glm::vec3 meshMax(std::numeric_limits<float>::lowest());

        // Extract vertex data
        for (unsigned int j = 0; j < mesh->mNumVertices; j++)
        {
            Vertex vertex{};

            //glm::vec4 pos = { mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z, 1.f };
            //vertex.position = glm::vec3(transform * pos);
            vertex.position = { mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z };

            // Update min and max vectors for AABB
            meshMin = glm::min(meshMin, vertex.position);
            meshMax = glm::max(meshMax, vertex.position);

            // Normals
            if (mesh->HasNormals())
            {
                //glm::mat4 normalMatrix = glm::transpose(glm::inverse(transform));
                //glm::vec4 norm = { mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z, 0.f };
                //vertex.normal = glm::normalize(glm::vec3(normalMatrix * norm));
                vertex.normal = { mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z };
            }

            // UV Coordinates
            if (mesh->HasTextureCoords(0))
            {
                vertex.uv = { mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y };
            }

            // Colors
            if (mesh->HasVertexColors(0))
            {
                vertex.color = { mesh->mColors[0][j].r, mesh->mColors[0][j].g, mesh->mColors[0][j].b };
            }

            newMesh.mVertices.push_back(vertex);
        }

        //Update model's AABB
        mAABBmin = glm::min(meshMin, mAABBmin);
        mAABBmax = glm::max(meshMax, mAABBmax);

        // Extract indices from faces
        for (unsigned int k = 0; k < mesh->mNumFaces; k++)
        {
            const aiFace& face = mesh->mFaces[k];
            for (unsigned int l = 0; l < face.mNumIndices; l++)
            {
                newMesh.mIndices.push_back(face.mIndices[l]);
            }
        }

        ProcessMaterials(mesh, newMesh);
        ExtractBoneWeights(newMesh.mVertices, mesh);

        return newMesh;
    }

    void Model::NormalizeModel()
    {
        glm::vec3 size = mAABBmax - mAABBmin;

        glm::vec3 center = (mAABBmax + mAABBmin) * 0.5f;
        float invScale = 1.f / std::max({ size.x, size.y, size.z });

        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), -center);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(invScale));

        mNormalizationMatrix = scaleMatrix * translationMatrix;
        mAnimationTransform = glm::vec4(center, invScale);
    }

    void Model::ProcessMaterials(aiMesh* mesh, IMesh& newMesh)
    {
        if (!mScene->HasMaterials()) return;

        aiMaterial* material = mScene->mMaterials[mesh->mMaterialIndex];
        ProcessBaseColor(mScene, material, newMesh);
        ProcessDiffuseTexture(mScene, material, newMesh);
    }

    void Model::ProcessBaseColor(const aiScene* mScene, aiMaterial* material, IMesh& newMesh)
    {
        aiColor4D baseColor;

        if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS ||
            material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS)
        {
            for (Vertex& vertex : newMesh.mVertices)
            {
                vertex.color *= glm::vec3(baseColor.r, baseColor.g, baseColor.b);
            }
        }
    }

    void Model::ProcessDiffuseTexture(const aiScene* mScene, aiMaterial* material, IMesh& newMesh)
    {
        aiString texturePath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) != AI_SUCCESS) return;

        const aiTexture* embeddedTexture = mScene->GetEmbeddedTexture(texturePath.C_Str());
        if (!embeddedTexture)
        {
            std::filesystem::path path(texturePath.C_Str());
            std::string filename = path.filename().string();

            newMesh.diffuseTexturePath = mDirectory + "ModelTextures/" + mModelName + "/" + filename;
        }
        else if (embeddedTexture->mHeight == 0)
        {
            newMesh.mTextureSize = static_cast<uint32_t>(embeddedTexture->mWidth);
            newMesh.mTextureData = std::make_unique<unsigned char[]>(newMesh.mTextureSize);

            std::memcpy(newMesh.mTextureData.get(), embeddedTexture->pcData, static_cast<size_t>(embeddedTexture->mWidth));
        }
        else
        {
            DOG_CRITICAL("How are we here");
        }
    }

    void Model::ExtractBoneWeights(std::vector<Vertex>& vertices, aiMesh* mesh)
    {
        // Iterate over all bones in the aiMesh
        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
        {
            aiBone* bone = mesh->mBones[boneIndex];
            std::string boneName = bone->mName.C_Str();

            int boneID = mSkeleton.GetOrCreateBoneID(boneName, aiMatToGlm(bone->mOffsetMatrix));

            // Update vertex weights
            for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
            {
                const aiVertexWeight& weightData = bone->mWeights[weightIndex];
                vertices[weightData.mVertexId].SetBoneData(boneID, weightData.mWeight);
            }
        }
    }
}
