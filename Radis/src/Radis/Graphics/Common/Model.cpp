/*****************************************************************//**
 * \file   Model.cpp
 * \brief  Implementation of the Model class, a model cooked by the asset pipeline.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "Model.h"
#include "CookedModelLoader.h"

namespace Radis
{
    Model::Model(const std::string& filePath)
    {
        const std::filesystem::path path(filePath);
        mDirectory = path.parent_path().string();
        mModelName = path.stem().string();

        if (Load(filePath))
        {
            NormalizeModel();
        }
    }

    Model::~Model()
    {
    }

    bool Model::Load(const std::string& path)
    {
        CookedModelData data;
        if (!CookedModelLoader::Load(path, data))
        {
            RADIS_CRITICAL("Failed to load cooked model {}", path);
            return false;
        }

        mMeshes = CookedModelLoader::CreateMeshes(data, Assets::ModelTexturesPath);
        mSkeleton = CookedModelLoader::CreateSkeleton(data);

        mAABBmin = glm::vec3(data.header.boundsMin[0], data.header.boundsMin[1], data.header.boundsMin[2]);
        mAABBmax = glm::vec3(data.header.boundsMax[0], data.header.boundsMax[1], data.header.boundsMax[2]);
        return true;
    }

    void Model::NormalizeModel()
    {
        const glm::vec3 size = mAABBmax - mAABBmin;
        const glm::vec3 center = (mAABBmax + mAABBmin) * 0.5f;
        const float largest = std::max({ size.x, size.y, size.z });
        if (largest <= 0.0f)
        {
            return;
        }

        const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), -center);
        const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / largest));
        mNormalizationMatrix = scaleMatrix * translationMatrix;
    }
}
