/*****************************************************************//**
 * \file   ModelLibrary.cpp
 * \author Adi (adityaprk04@gmail.com)
 * \date   October 8 2025
 
 * \brief  Library of Models
 *  *********************************************************************/

#include <PCH/pch.h>
#include "ModelLibrary.h"
#include "Model.h"
#include "UnifiedMesh.h"

#include "TextureLibrary.h"
#include "../Vulkan/Core/Device.h"
#include "../OpenGL/GLMesh.h"
#include "Engine.h"

namespace Dog
{
    const uint32_t ModelLibrary::INVALID_MODEL_INDEX = 10001;

    ModelLibrary::ModelLibrary(Device& device, TextureLibrary& textureLibrary)
        : mDevice{ device }
        , mTextureLibrary{ textureLibrary }
    {
        mUnifiedMesh = std::make_unique<UnifiedMeshes>();
    }

    ModelLibrary::~ModelLibrary()
    {
        mModels.clear();
    }

    uint32_t ModelLibrary::AddModel(const std::string& filePath)
    {
        auto it = mModelMap.find(filePath);
        if (it != mModelMap.end())
        {
            return it->second;
        }

        std::unique_ptr<Model> model = std::make_unique<Model>(mDevice, filePath);
        
        uint32_t modelID = static_cast<uint32_t>(mModels.size());
        mModels.push_back(std::move(model));
        //AddToUnifiedMesh(modelID);

        // std::string mModelName = std::filesystem::path(filePath).stem().string();
        mModelMap[filePath] = modelID;
        mLastModelLoaded = modelID;
        
        return modelID;
    }

    void ModelLibrary::AddToUnifiedMesh(uint32_t modelIndex)
    {
        if (modelIndex == INVALID_MODEL_INDEX)
        {
            return;
        }

        if (modelIndex >= mModels.size())
        {
            return;
        }

        Model* model = mModels[modelIndex].get();
        if (!model) return;

        for (auto& mesh : model->mMeshes)
        {
            mUnifiedMesh->AddMesh(mDevice, *mesh);
        }
    }

    Model* ModelLibrary::GetModel(uint32_t index)
    {
        if (index == INVALID_MODEL_INDEX || index >= mModels.size())
        {
            return nullptr;
        }

        return mModels[index].get();
    }

    Model* ModelLibrary::GetModel(const std::string& modelPath)
    {
        if (modelPath.empty()) return nullptr;

        auto it = mModelMap.find(modelPath);
        if (it == mModelMap.end()) return nullptr;
        return GetModel(it->second);
    }

    Model* ModelLibrary::TryAddGetModel(const std::string& modelPath)
    {
        if (modelPath.empty()) return nullptr;

        auto it = mModelMap.find(modelPath);
        if (it == mModelMap.end())
        {
            uint32_t newModelIndex = AddModel(modelPath);
            return GetModel(newModelIndex);
        }
        return GetModel(it->second);
    }

    uint32_t ModelLibrary::GetModelIndex(const std::string& modelPath)
    {
        auto it = mModelMap.find(modelPath);
        if (it == mModelMap.end())
        {
            return INVALID_MODEL_INDEX;
        }

        return it->second;
    }

    void ModelLibrary::LoadTextures()
    {
        for (auto& model : mModels)
        {
            if (model->mAddedTexture) continue;
            model->mAddedTexture = true;

            auto LoadOrGetTexture = [&](uint32_t& currentIndex, const std::string& path, std::unique_ptr<unsigned char[]>& data, uint32_t& dataSize, const std::string& embeddedName)
            {
                if (currentIndex != TextureLibrary::INVALID_TEXTURE_INDEX) return;
                
                if (!path.empty()) currentIndex = mTextureLibrary.AddTexture(path);
                else if (data != nullptr && dataSize > 0)
                {
                    currentIndex = mTextureLibrary.AddTexture(data.get(), dataSize, embeddedName);
                    data.reset();
                    dataSize = 0;
                }
            };

            // --- Process all meshes in the model ---
            for (auto& mesh : model->mMeshes)
            {
                // Create a unique base name for embedded textures for this mesh
                std::string embeddedBaseName = "Embedded_" + model->mModelName + "_" + std::to_string(mesh->mMeshID);

                // Call the helper for every texture type
                LoadOrGetTexture(mesh->albedoTextureIndex, mesh->albedoTexturePath, mesh->mAlbedoTextureData, mesh->mAlbedoTextureSize, embeddedBaseName + "_Albedo");
                LoadOrGetTexture(mesh->normalTextureIndex, mesh->normalTexturePath, mesh->mNormalTextureData, mesh->mNormalTextureSize, embeddedBaseName + "_Normal");
                LoadOrGetTexture(mesh->metalnessTextureIndex, mesh->metalnessTexturePath, mesh->mMetalnessTextureData, mesh->mMetalnessTextureSize, embeddedBaseName + "_Metalness");
                LoadOrGetTexture(mesh->roughnessTextureIndex, mesh->roughnessTexturePath, mesh->mRoughnessTextureData, mesh->mRoughnessTextureSize, embeddedBaseName + "_Roughness");
                LoadOrGetTexture(mesh->occlusionTextureIndex, mesh->occlusionTexturePath, mesh->mOcclusionTextureData, mesh->mOcclusionTextureSize, embeddedBaseName + "_Occlusion");
                LoadOrGetTexture(mesh->emissiveTextureIndex, mesh->emissiveTexturePath, mesh->mEmissiveTextureData, mesh->mEmissiveTextureSize, embeddedBaseName + "_Emissive");
            }
        }
    }

    bool ModelLibrary::NeedsTextureUpdate()
    {
        if (mLastModelLoaded == INVALID_MODEL_INDEX) return false;

        Model* model = GetModel(mLastModelLoaded);
        if (!model) return false;

        return !model->mAddedTexture;
    }

    void ModelLibrary::ClearAllBuffers(Device* device)
    {
        for (auto& model : mModels)
        {
            model->mAddedTexture = false;
            for (auto& mesh : model->mMeshes)
            {
                mesh->Cleanup();
            }
        }
    }

    void ModelLibrary::RecreateAllBuffers(Device* device)
    {
        for (auto& model : mModels)
        {
            for (auto& mesh : model->mMeshes)
            {
                std::vector<Vertex> oldVertices = mesh->mVertices;
                std::vector<uint32_t> oldIndices = mesh->mIndices;
                uint32_t oldMeshID = mesh->GetID();
                uint32_t oldDiffuseTextureIndex = mesh->albedoTextureIndex;

                mesh.reset();

                if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
                {
                    mesh = std::make_unique<GLMesh>(false);
                }
                else if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
                {
                    mesh = std::make_unique<VKMesh>(false);
                }

                mesh->mMeshID = oldMeshID;
                mesh->mVertices = oldVertices;
                mesh->mIndices = oldIndices;
                mesh->albedoTextureIndex = oldDiffuseTextureIndex;

                mesh->CreateVertexBuffers(device);
                mesh->CreateIndexBuffers(device);
            }
        }
    }
}
