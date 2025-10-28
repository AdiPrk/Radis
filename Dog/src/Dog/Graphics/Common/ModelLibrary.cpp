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

#include "../Vulkan/Texture/TextureLibrary.h"

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

        std::string lowerPath = filePath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

        std::unique_ptr<Model> model = std::make_unique<Model>(mDevice, lowerPath);
        
        uint32_t modelID = static_cast<uint32_t>(mModels.size());
        mModels.push_back(std::move(model));
        //AddToUnifiedMesh(modelID);

        // std::string mModelName = std::filesystem::path(filePath).stem().string();
        mModelMap[lowerPath] = modelID;
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
        if (index == INVALID_MODEL_INDEX)
        {
            return nullptr;
        }

        if (index >= mModels.size())
        {
            return nullptr;
        }

        return mModels[index].get();
    }

    Model* ModelLibrary::GetModel(const std::string& modelPath)
    {
        if (modelPath.empty()) return nullptr;

        std::string lowerPath = modelPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

        auto it = mModelMap.find(lowerPath);
        if (it == mModelMap.end()) return nullptr;
        return GetModel(it->second);
    }

    Model* ModelLibrary::TryAddGetModel(const std::string& modelPath)
    {
        if (modelPath.empty()) return nullptr;

        std::string lowerPath = modelPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

        auto it = mModelMap.find(lowerPath);
        if (it == mModelMap.end())
        {
            uint32_t newModelIndex = AddModel(lowerPath);
            return GetModel(newModelIndex);
        }
        return GetModel(it->second);
    }

    uint32_t ModelLibrary::GetModelIndex(const std::string& modelPath)
    {
        std::string lowerPath = modelPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

        auto it = mModelMap.find(modelPath);
        if (it == mModelMap.end())
        {
            return INVALID_MODEL_INDEX;
        }

        return it->second;
    }

    void ModelLibrary::LoadTextures()
    {
        /*
        for (auto& model : mModels)
        {
            if (model->mAddedTexture) continue;
            model->mAddedTexture = true;

            for (auto& mesh : model->mMeshes)
            {
                if (mesh->diffuseTexturePath.empty())
                {
                    if (!mesh->mTextureData || mesh->mTextureSize == 0) continue;

                    uint32_t ind = mTextureLibrary.AddTexture(mesh->mTextureData.get(), mesh->mTextureSize, "EmbeddedTexture_" + std::to_string(mesh->mMeshID));
                    mesh->diffuseTextureIndex = ind;
                    mesh->mTextureData.reset();
                    mesh->mTextureSize = 0;
                }
                else 
                {
                    uint32_t ind = mTextureLibrary.AddTexture(mesh->diffuseTexturePath);
                    mesh->diffuseTextureIndex = ind;
                }
            }
        }
        */
    }
    bool ModelLibrary::NeedsTextureUpdate()
    {
        if (mLastModelLoaded == INVALID_MODEL_INDEX) return false;

        Model* model = GetModel(mLastModelLoaded);
        if (!model) return false;

        return !model->mAddedTexture;
    }

    void ModelLibrary::RecreateAllBuffers()
    {
        for (auto& model : mModels)
        {
            for (auto& mesh : model->mMeshes)
            {
                
            }
        }
    }
}
