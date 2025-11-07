#include <PCH/pch.h>
#include "Model.h"
#include "../Vulkan/Core/Buffer.h"
#include "../Vulkan/VKMesh.h"
#include "../OpenGL/GLMesh.h"
#include "Assets/Assets.h"

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

    std::string Model::ResolveTexturePath(aiMaterial* material, const std::vector<aiTextureType>& typesToTry, std::unique_ptr<unsigned char[]>& outEmbeddedData, uint32_t& outEmbeddedDataSize)
    {
        aiString texturePath;
        aiReturn result = AI_FAILURE;

        for (aiTextureType type : typesToTry)
        {
            // Assimp materials can have multiple textures of the same type.
            // For PBR, we almost always only care about the first one (index 0).
            if (material->GetTexture(type, 0, &texturePath) == AI_SUCCESS)
            {
                result = AI_SUCCESS;
                break;
            }
        }

        if (result != AI_SUCCESS)
        {
            return "";
        }

        // --- We found a texture, now resolve its path ---

        const aiTexture* embeddedTexture = mScene->GetEmbeddedTexture(texturePath.C_Str());
        bool embedded = false;
        if (!embeddedTexture)
        {
            // Not an embedded texture. This is an external file.
            std::filesystem::path path(texturePath.C_Str());
            std::string filename = path.filename().string();

            return Assets::ModelTexturesPath + mModelName + "/" + filename;
        }
        else if (embeddedTexture->mHeight == 0)
        {
            // This is compressed, "non-pixel" data (e.g., JPEG, PNG buffer)
            embedded = true;
            outEmbeddedDataSize = static_cast<uint32_t>(embeddedTexture->mWidth);
            outEmbeddedData = std::make_unique<unsigned char[]>(outEmbeddedDataSize);
            std::memcpy(outEmbeddedData.get(), embeddedTexture->pcData, outEmbeddedDataSize);
        }
        else
        {
            DOG_CRITICAL("Model has weird embedded texture data (?)?(?) what does this even mean");
            return "";
        }

        return embedded ? "" : texturePath.C_Str();
    }

    void Model::ProcessMaterials(aiMesh* mesh, IMesh& newMesh)
    {
        if (!mScene->HasMaterials()) return;

        aiMaterial* material = mScene->mMaterials[mesh->mMaterialIndex];
        //ProcessVertexColor(material, newMesh);
        ProcessBaseColor(material, newMesh);
        ProcessNormalMap(material, newMesh);
        ProcessPBRMaps(material, newMesh);
        ProcessEmissive(material, newMesh);
    }

    void Model::ProcessVertexColor(aiMaterial* material, IMesh& newMesh)
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

    void Model::ProcessBaseColor(aiMaterial* material, IMesh& newMesh)
    {
        aiColor4D color;

        if (material->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS)
        {
            newMesh.baseColorFactor = glm::vec4(color.r, color.g, color.b, color.a);
        }
        else if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
        {
            newMesh.baseColorFactor = glm::vec4(color.r, color.g, color.b, color.a);
        }

        newMesh.albedoTexturePath = ResolveTexturePath(
            material,
            { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE },
            newMesh.mAlbedoTextureData,
            newMesh.mAlbedoTextureSize
        );
    }

    void Model::ProcessNormalMap(aiMaterial* material, IMesh& newMesh)
    {
        newMesh.normalTexturePath = ResolveTexturePath(
            material,
            { aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS },
            newMesh.mNormalTextureData,
            newMesh.mNormalTextureSize
        );
    }

    void Model::ProcessPBRMaps(aiMaterial* material, IMesh& newMesh)
    {
        material->Get(AI_MATKEY_METALLIC_FACTOR, newMesh.metallicFactor);
        material->Get(AI_MATKEY_ROUGHNESS_FACTOR, newMesh.roughnessFactor);

        newMesh.metalnessTexturePath = ResolveTexturePath(
            material,
            { aiTextureType_METALNESS },
            newMesh.mMetalnessTextureData,
            newMesh.mMetalnessTextureSize
        );

        newMesh.roughnessTexturePath = ResolveTexturePath(
            material,
            { aiTextureType_DIFFUSE_ROUGHNESS },
            newMesh.mRoughnessTextureData,
            newMesh.mRoughnessTextureSize
        );

        newMesh.occlusionTexturePath = ResolveTexturePath(
            material,
            { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP },
            newMesh.mOcclusionTextureData,
            newMesh.mOcclusionTextureSize
        );

        // Your *engine* can now check the paths
        if (!newMesh.metalnessTexturePath.empty() && newMesh.metalnessTexturePath == newMesh.roughnessTexturePath)
        {
            // The paths are identical!
            // This could be an ORM texture (Occlusion-Roughness-Metalness) if glTF
        }
    }
        
    void Model::ProcessEmissive(aiMaterial* material, IMesh& newMesh)
    {
        aiColor3D color(0.0f, 0.0f, 0.0f);
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
        {
            newMesh.emissiveFactor = glm::vec4(glm::vec3(color.r, color.g, color.b), 0.f);
        }

        newMesh.emissiveTexturePath = ResolveTexturePath(
            material,
            { aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE },
            newMesh.mEmissiveTextureData,
            newMesh.mEmissiveTextureSize
        );
    }
}
