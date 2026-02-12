/*****************************************************************//**
 * \file   ComputePipeline.cpp
 * \brief  Implementation of the ComputePipeline class for Vulkan compute pipelines.
 *
 * \author Aditya Prakash
 * \date   February 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "ComputePipeline.h"
#include "VKShader.h"

#include "../Core/Device.h"

#include "../Uniform/Uniform.h"
#include "../Uniform/Descriptors.h"

namespace Radis
{
    ComputePipeline::ComputePipeline(Device& device, const std::vector<Uniform*>& uniforms, const std::string& compFile, const std::string& entryPoint)
        : device(device)
        , mCompPath(ShaderDir + compFile)
        , mSpvCompPath(SpvDir + compFile + ".spv")
        , mEntryPoint(entryPoint)
        , mUniforms(uniforms)
    {
        CreatePipelineLayout(uniforms);
        CreatePipeline();
    }

    ComputePipeline::~ComputePipeline()
    {
        DestroyPipeline();

        if (mPipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device.GetDevice(), mPipelineLayout, nullptr);
            mPipelineLayout = VK_NULL_HANDLE;
        }
    }

    void ComputePipeline::DestroyPipeline()
    {
        if (mComputePipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device.GetDevice(), mComputePipeline, nullptr);
            mComputePipeline = VK_NULL_HANDLE;
        }

        if (mCompShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device.GetDevice(), mCompShaderModule, nullptr);
            mCompShaderModule = VK_NULL_HANDLE;
        }
    }

    void ComputePipeline::Recreate()
    {
        vkDeviceWaitIdle(device.GetDevice());

        DestroyPipeline();

        if (!mCompPath.empty()) Shader::CompileShader(mCompPath);

        CreatePipeline();
    }

    void ComputePipeline::Bind(VkCommandBuffer commandBuffer)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputePipeline);
    }

    void ComputePipeline::CreatePipelineLayout(const std::vector<Uniform*>& uniforms)
    {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        // Set uniform binding indexes and gather descriptor set layouts
        std::vector<VkDescriptorSetLayout> uniformsDescriptorSetLayouts;
        uniformsDescriptorSetLayouts.reserve(uniforms.size());

        for (int i = 0; i < static_cast<int>(uniforms.size()); ++i)
        {
            uniforms[i]->SetBinding(i);
            uniformsDescriptorSetLayouts.push_back(
                uniforms[i]->GetDescriptorLayout()->GetDescriptorSetLayout()
            );
        }

        pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(uniformsDescriptorSetLayouts.size());
        pipelineLayoutCreateInfo.pSetLayouts = uniformsDescriptorSetLayouts.data();

        // Push constants (none for now, same as your Pipeline)
        pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device.GetDevice(), &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
        {
            RADIS_CRITICAL("Failed to create compute pipeline layout");
        }
    }

    void ComputePipeline::CreatePipeline()
    {
        if (mPipelineLayout == VK_NULL_HANDLE)
        {
            RADIS_CRITICAL("Cannot create compute pipeline before pipeline layout");
        }

        LoadComputeShaderModule();
        CreateComputePipeline();
    }

    void ComputePipeline::LoadComputeShaderModule()
    {
        // Read SPV file
        std::ifstream compSPVFile(mSpvCompPath, std::ios::binary);

        if (!compSPVFile.is_open())
        {
            RADIS_ERROR("Failed to open compute SPV file: {}", mSpvCompPath);
            return;
        }

        compSPVFile.seekg(0, std::ios::end);
        size_t compSPVFileSize = static_cast<size_t>(compSPVFile.tellg());
        compSPVFile.seekg(0, std::ios::beg);

        std::vector<uint32_t> compShaderSPV(compSPVFileSize / sizeof(uint32_t));
        compSPVFile.read(reinterpret_cast<char*>(compShaderSPV.data()), compSPVFileSize);
        compSPVFile.close();

        Shader::CreateShaderModule(device, compShaderSPV, &mCompShaderModule);

        if (mCompShaderModule == VK_NULL_HANDLE)
        {
            RADIS_ERROR("Failed to create compute shader module from: {}", mSpvCompPath);
        }
    }

    void ComputePipeline::CreateComputePipeline()
    {
        if (mCompShaderModule == VK_NULL_HANDLE)
        {
            RADIS_ERROR("Cannot create compute pipeline: compute shader module is null");
            return;
        }

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = mCompShaderModule;
        stageInfo.pName = mEntryPoint.c_str();
        stageInfo.pNext = nullptr;
        stageInfo.flags = 0;
        stageInfo.pSpecializationInfo = nullptr;

        VkComputePipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stage = stageInfo;
        pipelineCreateInfo.layout = mPipelineLayout;

        pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineCreateInfo.basePipelineIndex = -1;

        if (vkCreateComputePipelines(device.GetDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mComputePipeline) != VK_SUCCESS)
        {
            RADIS_ERROR("Failed to create compute pipeline");
        }
    }
}
