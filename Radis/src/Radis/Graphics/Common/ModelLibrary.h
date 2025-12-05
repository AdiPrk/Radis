/*****************************************************************//**
 * \file   ModelLibrary.hpp
 * \author Adi (aditya.prakash@digipen.edu)
 * \date   September 26 2024
 * \Copyright @ 2024 Digipen (USA) Corporation *

 * \brief  Library for models
 *  *********************************************************************/
#pragma once

#include "../Vulkan/Core/Device.h"
#include "Assets/CaseInsensitiveHash.h"

namespace Radis
{
	class Model;
	class Uniform;
	class TextureLibrary;
    class UnifiedMeshes;

	class ModelLibrary
	{
	public:
		ModelLibrary(Device& device, TextureLibrary& textureLibrary);
		~ModelLibrary();

        uint32_t AddModel(const std::string& modelPath, bool fromDM = false, bool toDM = false);
        void AddToUnifiedMesh(uint32_t modelIndex);

        Model* GetModel(uint32_t index);
        Model* GetModel(const std::string& modelPath);
		Model* TryAddGetModel(const std::string& modelPath);
		uint32_t GetModelIndex(const std::string& modelPath);
        UnifiedMeshes* GetUnifiedMesh() { return mUnifiedMesh.get(); }

        uint32_t GetModelCount() const { return static_cast<uint32_t>(mModels.size()); }
		
		void QueueTextures();
        const static uint32_t INVALID_MODEL_INDEX;

		void ClearAllBuffers(class Device* device);
		void RecreateAllBuffers(class Device* device);

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