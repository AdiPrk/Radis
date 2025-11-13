#include <PCH/pch.h>
#include "RaytracingPipeline.h"
#include "../Core/Device.h"
#include "../Core/Allocator.h"
#include "../Uniform/Uniform.h"
#include "../Uniform/Descriptors.h"
#include "VKShader.h"

namespace Dog
{
	RaytracingPipeline::RaytracingPipeline(Device& device, const std::vector<Uniform*>& uniforms)
		: device(device)
	{
        // Creating all shaders (placeholder for now)
        enum StageIndices
        {
            eRaygen,
            eMiss,
            eShadowMiss,
            eClosestHit,
            eShaderGroupCount
        };
        std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
        for (auto& s : stages)
            s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        std::ifstream rgenSPVFile("Assets/Shaders/spv/raytrace.rgen.spv", std::ios::binary);
        std::ifstream missSPVFile("Assets/Shaders/spv/raytrace.rmiss.spv", std::ios::binary);
        std::ifstream shadowMissSPVFile("Assets/Shaders/spv/shadow.rmiss.spv", std::ios::binary);
        std::ifstream chitSPVFile("Assets/Shaders/spv/raytrace.rchit.spv", std::ios::binary);

        if (rgenSPVFile.is_open() && missSPVFile.is_open() && chitSPVFile.is_open())
        {
            // read spv files
            rgenSPVFile.seekg(0, std::ios::end);
            size_t rgenSPVFileSize = rgenSPVFile.tellg();
            rgenSPVFile.seekg(0, std::ios::beg);
            std::vector<uint32_t> rgenShaderSPV(rgenSPVFileSize / sizeof(uint32_t));
            rgenSPVFile.read(reinterpret_cast<char*>(rgenShaderSPV.data()), rgenSPVFileSize);
            rgenSPVFile.close();

            missSPVFile.seekg(0, std::ios::end);
            size_t missSPVFileSize = missSPVFile.tellg();
            missSPVFile.seekg(0, std::ios::beg);
            std::vector<uint32_t> missShaderSPV(missSPVFileSize / sizeof(uint32_t));
            missSPVFile.read(reinterpret_cast<char*>(missShaderSPV.data()), missSPVFileSize);
            missSPVFile.close();

            shadowMissSPVFile.seekg(0, std::ios::end);
            size_t shadowMissSPVFileSize = shadowMissSPVFile.tellg();
            shadowMissSPVFile.seekg(0, std::ios::beg);
            std::vector<uint32_t> shadowMissShaderSPV(shadowMissSPVFileSize / sizeof(uint32_t));
            shadowMissSPVFile.read(reinterpret_cast<char*>(shadowMissShaderSPV.data()), shadowMissSPVFileSize);
            shadowMissSPVFile.close();

            chitSPVFile.seekg(0, std::ios::end);
            size_t chitSPVFileSize = chitSPVFile.tellg();
            chitSPVFile.seekg(0, std::ios::beg);
            std::vector<uint32_t> chitShaderSPV(chitSPVFileSize / sizeof(uint32_t));
            chitSPVFile.read(reinterpret_cast<char*>(chitShaderSPV.data()), chitSPVFileSize);
            chitSPVFile.close();

            Shader::CreateShaderModule(device, rgenShaderSPV, &mRgenShaderModule);
            Shader::CreateShaderModule(device, missShaderSPV, &mMissShaderModule);
            Shader::CreateShaderModule(device, shadowMissShaderSPV, &mShadowMissShaderModule);
            Shader::CreateShaderModule(device, chitShaderSPV, &mChitShaderModule);
        }
        else
        {
            DOG_CRITICAL("Failed to open ray tracing shader SPV files!");
            return;
        }

        stages[eRaygen].module = mRgenShaderModule;
        stages[eRaygen].pName = "main";
        stages[eRaygen].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        stages[eMiss].module = mMissShaderModule;
        stages[eMiss].pName = "main";
        stages[eMiss].stage = VK_SHADER_STAGE_MISS_BIT_KHR;

        stages[eShadowMiss].module = mShadowMissShaderModule;
        stages[eShadowMiss].pName = "main";
        stages[eShadowMiss].stage = VK_SHADER_STAGE_MISS_BIT_KHR;

        stages[eClosestHit].module = mChitShaderModule;
        stages[eClosestHit].pName = "main";
        stages[eClosestHit].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

        // Shader groups
        VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
        // Raygen
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = eRaygen;
        shader_groups.push_back(group);

        // Miss
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = eMiss;
        shader_groups.push_back(group);

        // Group 2: Miss (Shadow)
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = eShadowMiss;
        shader_groups.push_back(group);

        // closest hit shader
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = eClosestHit;
        shader_groups.push_back(group);

        // Push constant: we want to be able to update constants used by the shaders
        // const VkPushConstantRange push_constant{ VK_SHADER_STAGE_ALL, 0, sizeof(TutoPushConstant) };
        // 
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        // pipeline_layout_create_info.pushConstantRangeCount = 1;
        // pipeline_layout_create_info.pPushConstantRanges = &push_constant;

        // Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
        std::vector<VkDescriptorSetLayout> layouts;
        for (int i = 0; i < uniforms.size(); ++i)
        {
            uniforms[i]->SetBinding(i);
            layouts.push_back(uniforms[i]->GetDescriptorLayout()->GetDescriptorSetLayout());
        }

        pipeline_layout_create_info.setLayoutCount = uint32_t(layouts.size());
        pipeline_layout_create_info.pSetLayouts = layouts.data();
        vkCreatePipelineLayout(device.GetDevice(), &pipeline_layout_create_info, nullptr, &mRtPipelineLayout);

        VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
        rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
        rtPipelineInfo.pStages = stages.data();
        rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
        rtPipelineInfo.pGroups = shader_groups.data();
        rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(3U, device.GetRayTracingProperties().maxRayRecursionDepth);
        rtPipelineInfo.layout = mRtPipelineLayout;
        device.g_vkCreateRayTracingPipelinesKHR(device.GetDevice(), {}, {}, 1, &rtPipelineInfo, nullptr, &mRtPipeline);

        DOG_INFO("Ray tracing pipeline layout created successfully");

        // Create the shader binding table for this pipeline
        CreateShaderBindingTable(rtPipelineInfo);
	}

	RaytracingPipeline::~RaytracingPipeline()
	{
        Destroy();
	}

	void RaytracingPipeline::Destroy()
	{
        if (mRtPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device.GetDevice(), mRtPipeline, nullptr);
            mRtPipeline = VK_NULL_HANDLE;
        }

        if (mRtPipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device.GetDevice(), mRtPipelineLayout, nullptr);
            mRtPipelineLayout = VK_NULL_HANDLE;
        }

        Allocator::DestroyBuffer(mSbtBuffer);

        if (mRgenShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device.GetDevice(), mRgenShaderModule, nullptr);
            mRgenShaderModule = VK_NULL_HANDLE;
        }
        if (mMissShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device.GetDevice(), mMissShaderModule, nullptr);
            mMissShaderModule = VK_NULL_HANDLE;
        }
        if (mShadowMissShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device.GetDevice(), mShadowMissShaderModule, nullptr);
            mShadowMissShaderModule = VK_NULL_HANDLE;
        }
        if (mChitShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device.GetDevice(), mChitShaderModule, nullptr);
            mChitShaderModule = VK_NULL_HANDLE;
        }
	}

	void RaytracingPipeline::Recreate()
	{
		Destroy();

    }

    void RaytracingPipeline::CreateShaderBindingTable(const VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo)
    {
        uint32_t handleSize = device.GetRayTracingProperties().shaderGroupHandleSize;
        uint32_t handleAlignment = device.GetRayTracingProperties().shaderGroupHandleAlignment;
        uint32_t baseAlignment = device.GetRayTracingProperties().shaderGroupBaseAlignment;
        uint32_t groupCount = rtPipelineInfo.groupCount;

        // Get shader group handles
        size_t dataSize = handleSize * groupCount;
        mShaderHandles.resize(dataSize);
        device.g_vkGetRayTracingShaderGroupHandlesKHR(device.GetDevice(), mRtPipeline, 0, groupCount, dataSize, mShaderHandles.data());

        // Calculate SBT buffer size with proper alignment
        auto     alignUp = [](uint32_t size, uint32_t alignment) { return (size + alignment - 1) & ~(alignment - 1); };
        uint32_t handleSizeAligned = alignUp(handleSize, handleAlignment);
        uint32_t raygenSize = handleSizeAligned;
        uint32_t missSize = handleSizeAligned * 2; // Two miss shaders
        uint32_t hitSize = handleSizeAligned;
        uint32_t callableSize = 0;  // No callable shaders in this tutorial

        // Ensure each region starts at a baseAlignment boundary
        uint32_t raygenOffset = 0;
        uint32_t missOffset = alignUp(raygenSize, baseAlignment);
        uint32_t hitOffset = alignUp(missOffset + missSize, baseAlignment);
        uint32_t callableOffset = alignUp(hitOffset + hitSize, baseAlignment);

        size_t bufferSize = callableOffset + callableSize;

        // Create SBT buffer
        Allocator::CreateBuffer(mSbtBuffer, bufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_SHADER_BINDING_TABLE_BIT_KHR, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
        Allocator::SetAllocationName(mSbtBuffer.allocation, "Ray Tracing SBT Buffer");

        // Populate SBT buffer
        uint8_t* pData = static_cast<uint8_t*>(mSbtBuffer.mapping);

        // Ray generation shader (group 0)
        memcpy(pData + raygenOffset, mShaderHandles.data() + 0 * handleSize, handleSize);
        mRaygenRegion.deviceAddress = mSbtBuffer.address + raygenOffset;
        mRaygenRegion.stride = raygenSize;
        mRaygenRegion.size = raygenSize;

        // Miss shader (group 1)
        memcpy(pData + missOffset, mShaderHandles.data() + 1 * handleSize, handleSize);
        memcpy(pData + missOffset + handleSizeAligned, mShaderHandles.data() + 2 * handleSize, handleSize);
        mMissRegion.deviceAddress = mSbtBuffer.address + missOffset;
        mMissRegion.stride = handleSizeAligned; // Stride is the size of ONE record
        mMissRegion.size = missSize;            // Size is TOTAL size (2 records)

        // Hit shader (group 2)
        memcpy(pData + hitOffset, mShaderHandles.data() + 3 * handleSize, handleSize);
        mHitRegion.deviceAddress = mSbtBuffer.address + hitOffset;
        mHitRegion.stride = hitSize;
        mHitRegion.size = hitSize;

        // Callable shaders (none in this tutorial)
        mCallableRegion.deviceAddress = 0;
        mCallableRegion.stride = 0;
        mCallableRegion.size = 0;

        DOG_INFO("Shader binding table created and populated");
    }

    void RaytracingPipeline::Bind(VkCommandBuffer cmd)
    {

    }
}
