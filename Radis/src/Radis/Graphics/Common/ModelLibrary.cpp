/*****************************************************************//**
 * \file   ModelLibrary.cpp
 * \brief  Implementation of the ModelLibrary class for managing 3D models.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "ModelLibrary.h"
#include "Model.h"
#include "UnifiedMesh.h"
#include "ECS/Components/Components.h"
#include "ECS/Resources/Assets/AssetResource.h"

#include "TextureLibrary.h"
#include "../Vulkan/Core/Device.h"
#include "Engine.h"

namespace Radis
{
    const uint32_t ModelLibrary::INVALID_MODEL_INDEX = UINT32_MAX;

    ModelLibrary::ModelLibrary(Device& device, TextureLibrary& textureLibrary)
        : mDevice{ device }
        , mTextureLibrary{ textureLibrary }
    {
        mUnifiedMesh = std::make_unique<UnifiedMeshes>();
    }

    ModelLibrary::~ModelLibrary()
    {
        ClearAllBuffers();
        mModels.clear();
    }

    uint32_t ModelLibrary::AddModel(const std::string& filePath, bool fromDM, bool toDM, bool yUp)
    {
        auto it = mModelMap.find(filePath);
        if (it != mModelMap.end())
        {
            return it->second;
        }

        ModelConfig config;
        config.fromDM = fromDM;
        config.toDM = toDM;
        config.yUp = yUp;

        std::unique_ptr<Model> model = std::make_unique<Model>(mDevice, filePath, config);
        for (auto& mesh : model->mMeshes)
        {
            mesh->UploadToGPU(&mDevice);
        }
        
        uint32_t modelID = static_cast<uint32_t>(mModels.size());
        mModels.push_back(std::move(model));
        // AddToUnifiedMesh(modelID);

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

    void ModelLibrary::InitializeUnifiedMesh()
    {
        mUnifiedMesh->AddModels(mDevice, mModels);
    }

    void ModelLibrary::SetUnifiedMesh()
    {
        mUnifiedMesh->SetModels(mDevice, mModels);
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

    Model* ModelLibrary::GetModel(ModelComponent& mc)
    {
        if (mc.UpdateModelID)
        {
            mc.UpdateModelID = false;
            Model* model = GetModel(mc.ModelPath);
            if (model)
            {
                mc.ModelID = GetModelIndex(mc.ModelPath);
            }
            else
            {
                mc.ModelID = INVALID_MODEL_INDEX;
            }
            return model;
        }
        else
        {
            return GetModel(mc.ModelID);
        }
    }

    Model* ModelLibrary::TryAddGetModel(ModelComponent& mc)
    {
        if (mc.UpdateModelID)
        {
            mc.UpdateModelID = false;
            Model* model = TryAddGetModel(mc.ModelPath);
            if (model)
            {
                mc.ModelID = GetModelIndex(mc.ModelPath);
            }
            else
            {
                mc.ModelID = INVALID_MODEL_INDEX;
            }
            return model;
        }
        else
        {
            return GetModel(mc.ModelID);
        }
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

    void ModelLibrary::QueueTextures()
    {
        for (auto& model : mModels)
        {
            if (model->mAddedTexture) continue;
            model->mAddedTexture = true;

            auto LoadOrGetTexture = [&](uint32_t& currentIndex, const std::string& path, std::vector<unsigned char>& data, const std::string& embeddedName)
            {
                if (currentIndex != TextureLibrary::INVALID_TEXTURE_INDEX) return;

                if (!path.empty()) 
                {
                    // currentIndex = assetResource->load<TextureData>(path);
                    currentIndex = mTextureLibrary.QueueTextureLoad(path);
                }
                else if (!data.empty())
                {
                    // currentIndex = assetResource->loadFromMemory<TextureData>(embeddedName, std::move(data));
                    currentIndex = mTextureLibrary.QueueTextureLoad(data.data(), static_cast<uint32_t>(data.size()), embeddedName);
                    data.clear();
                }
            };

            // --- Process all meshes in the model ---
            for (auto& mesh : model->mMeshes)
            {
                // Create a unique base name for embedded textures for this mesh
                std::string embeddedBaseName = "Embedded_" + model->mModelName + "_" + std::to_string(mesh->mMeshID);

                // Call the helper for every texture type
                LoadOrGetTexture(mesh->albedoTextureIndex, mesh->albedoTexturePath, mesh->mAlbedoTextureData, embeddedBaseName + "_Albedo");
                LoadOrGetTexture(mesh->normalTextureIndex, mesh->normalTexturePath, mesh->mNormalTextureData, embeddedBaseName + "_Normal");
                LoadOrGetTexture(mesh->metalnessTextureIndex, mesh->metalnessTexturePath, mesh->mMetalnessTextureData, embeddedBaseName + "_Metalness");
                LoadOrGetTexture(mesh->roughnessTextureIndex, mesh->roughnessTexturePath, mesh->mRoughnessTextureData, embeddedBaseName + "_Roughness");
                LoadOrGetTexture(mesh->occlusionTextureIndex, mesh->occlusionTexturePath, mesh->mOcclusionTextureData, embeddedBaseName + "_Occlusion");
                LoadOrGetTexture(mesh->emissiveTextureIndex, mesh->emissiveTexturePath, mesh->mEmissiveTextureData, embeddedBaseName + "_Emissive");
            }
        }
    }

    void ModelLibrary::ClearAllBuffers()
    {
        for (auto& model : mModels)
        {
            for (auto& mesh : model->mMeshes)
            {
                mesh->ReleaseGPU();
            }
        }

        mUnifiedMesh->GetUnifiedMesh().ReleaseGPU();
    }

    void ModelLibrary::RecreateAllBuffers(Device* device)
    {
        for (auto& model : mModels)
        {
            for (auto& mesh : model->mMeshes)
            {
                mesh->RecreateBuffer(device);
            }
        }

        mUnifiedMesh->GetUnifiedMesh().RecreateBuffer(device);
    }
}
