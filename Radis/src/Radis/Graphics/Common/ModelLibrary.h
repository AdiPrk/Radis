/*****************************************************************//**
 * \file   ModelLibrary.h
 * \brief Definition of the ModelLibrary class for managing 3D models.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "../Vulkan/Core/Device.h"
#include "Assets/CaseInsensitiveHash.h"

namespace Radis
{
	class Model;
	class Uniform;
	class TextureLibrary;
    class UnifiedMeshes;
	struct ModelComponent;

	class ModelLibrary
	{
	public:
		ModelLibrary(Device& device, TextureLibrary& textureLibrary);
		~ModelLibrary();

        uint32_t AddModel(const std::string& modelPath, bool fromDM = false, bool toDM = false, bool yUp = true);
        void AddToUnifiedMesh(uint32_t modelIndex);
		void UpdateUnifiedMesh();

        Model* GetModel(uint32_t index);
        Model* GetModel(const std::string& modelPath);
		Model* TryAddGetModel(const std::string& modelPath);

        Model* GetModel(ModelComponent& mc);
		Model* TryAddGetModel(ModelComponent& mc);

		uint32_t GetModelIndex(const std::string& modelPath);
        
		UnifiedMeshes* GetUnifiedMesh() { return mUnifiedMesh.get(); }

        uint32_t GetModelCount() const { return static_cast<uint32_t>(mModels.size()); }
		
		void QueueTextures();
        const static uint32_t INVALID_MODEL_INDEX;

		void ClearAllBuffers(class Device* device);
		void RecreateAllBuffers(class Device* device);

        const auto& GetModelMap() const { return mModelMap; }

	private:
		friend class Model;

		std::vector<std::unique_ptr<Model>> mModels;
		std::unordered_map<LowerCaseString, uint32_t, LowerCaseHash> mModelMap;

        std::unique_ptr<UnifiedMeshes> mUnifiedMesh;

		Device& mDevice;
        TextureLibrary& mTextureLibrary;

        uint32_t mLastModelLoaded = INVALID_MODEL_INDEX;
	};

} // namespace Radis