/*****************************************************************//**
 * \file   Model.h
 * \brief  Definition of the Model class, a model cooked by the asset pipeline.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "../Common/Animation/Skeleton.h"
#include "../RHI/Mesh.h"

namespace Radis
{
    class Model
    {
    public:
        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        // Loads a model cooked by the asset pipeline (a .dm file). A model that fails to load has
        // no meshes; the error is logged.
        explicit Model(const std::string& filePath);
        ~Model();

        std::vector<std::unique_ptr<Mesh>> mMeshes;

        // The skeleton of a skinned model; nullptr otherwise.
        const Skeleton* GetSkeleton() const { return mSkeleton.get(); }

        const std::string& GetName() const { return mModelName; }
        const std::string& GetDir() const { return mDirectory; }

        // Scales the model to fit a unit cube centered on the origin.
        const glm::mat4& GetNormalizationMatrix() const { return mNormalizationMatrix; }

    private:
        bool Load(const std::string& path);
        void NormalizeModel();

        glm::vec3 mAABBmin{ 0.0f };
        glm::vec3 mAABBmax{ 0.0f };

        friend class ModelLibrary;
        bool mAddedTexture = false;
        std::string mModelName;
        std::string mDirectory;

        glm::mat4 mNormalizationMatrix{ 1.0f };

        std::unique_ptr<Skeleton> mSkeleton;
    };
}
