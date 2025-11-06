#include <PCH/pch.h>
#include "RaytracingPipeline.h"
#include "../Core/Device.h"
#include "../Core/Allocator.h"
#include "../Uniform/Uniform.h"
#include "../Uniform/Descriptors.h"

namespace Dog
{
    struct TutoPushConstant
    {
        glm::mat3       normalMatrix;
        int            instanceIndex;              // Instance index for the current draw call
        void* sceneInfoAddress;           // Address of the scene information buffer
        glm::vec2      metallicRoughnessOverride;  // Metallic and roughness override values
    };

	RaytracingPipeline::RaytracingPipeline(Device& device, const std::vector<Uniform*>& uniforms)
		: device(device)
	{
        // Creating all shaders (placeholder for now)
        enum StageIndices
        {
            eRaygen,
            eMiss,
            eClosestHit,
            eShaderGroupCount
        };
        std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
        for (auto& s : stages)
            s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        // TODO: In Phase 5, we'll add actual shader compilation
        // For now, create empty stages to test pipeline creation
        DOG_INFO("Creating ray tracing pipeline structure (shaders will be added in Phase 5)\n");

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

        // closest hit shader
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = eClosestHit;
        shader_groups.push_back(group);

        // Push constant: we want to be able to update constants used by the shaders
        const VkPushConstantRange push_constant{ VK_SHADER_STAGE_ALL, 0, sizeof(TutoPushConstant) };

        VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges = &push_constant;

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

        // TODO: In Phase 5, we'll add actual shader stages and create the pipeline
        // For now, just log that the pipeline layout is ready
        DOG_INFO("Ray tracing pipeline layout created successfully\n");

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
	}

	void RaytracingPipeline::Recreate()
	{
		Destroy();

    }

    void RaytracingPipeline::CreateShaderBindingTable(const VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo)
    {
        // Calculate required SBT buffer size (will be populated in Phase 5)
        size_t bufferSize = 1024; // Placeholder size

        // Create SBT buffer
        Allocator::CreateBuffer(mSbtBuffer, bufferSize, VK_BUFFER_USAGE_2_SHADER_BINDING_TABLE_BIT_KHR, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
        Allocator::SetAllocationName(mSbtBuffer.allocation, "Ray Tracing Shader Binding Table");
        
        DOG_INFO("Shader binding table buffer created (will be populated in Phase 5)\n");
    }
}
