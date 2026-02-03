/*****************************************************************//**
 * \file   ComputePipeline.h
 * \brief  Definition of the ComputePipeline class for Vulkan compute pipelines.
 *
 * \author Aditya Prakash
 * \date   February 2026
 *********************************************************************/

#pragma once

#include "../Core/Device.h"

namespace Radis
{
    class Uniform;
    class Device;

    class ComputePipeline
    {
    public:
        // Shader Directories
        inline static const std::string ShaderDir = "Assets/shaders/";
        inline static const std::string SpvDir = "Assets/shaders/spv/";

        ComputePipeline(Device& device, const std::vector<Uniform*>& uniforms, const std::string& compFile, const std::string& entryPoint = "main");
        ~ComputePipeline();

        ComputePipeline(const ComputePipeline&) = delete;
        ComputePipeline& operator=(const ComputePipeline&) = delete;

        void DestroyPipeline();
        void Recreate();

        void Bind(VkCommandBuffer commandBuffer);

        VkPipelineLayout& GetLayout() { return mPipelineLayout; }
        VkPipeline GetPipeline() const { return mComputePipeline; }

    private:
        void CreatePipelineLayout(const std::vector<Uniform*>& uniforms);
        void CreatePipeline();
        void CreateComputePipeline();

        void LoadComputeShaderModule(); // reads SPV + creates VkShaderModule

    private:
        Device& device;

        VkPipeline       mComputePipeline = VK_NULL_HANDLE;
        VkShaderModule   mCompShaderModule = VK_NULL_HANDLE;
        VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;

        // Shader paths
        std::string mCompPath;
        std::string mSpvCompPath;
        std::string mEntryPoint;

        // Uniforms
        const std::vector<Uniform*>& mUniforms;
    };
}
