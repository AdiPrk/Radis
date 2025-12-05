#include <PCH/pch.h>

#include "Uniform.h"
#include "Descriptors.h"
#include "UniformSettings.h"
#include "ECS/Resources/RenderingResource.h"
#include "../Core/Device.h"
#include "../Core/SwapChain.h"
#include "../Core/Buffer.h"
#include "../Core/AccelerationStructures.h"


namespace Dog
{
    Uniform::Uniform(Device& device, RenderingResource& renderData, const UniformSettings& settings)
        : mDevice(device)
    {
        DescriptorPool::Builder poolBuilder = DescriptorPool::Builder(device).SetMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
        for (const auto& bindingInfo : settings.bindings)
        {
            poolBuilder.AddPoolSize(
                bindingInfo.layoutBinding.descriptorType,
                SwapChain::MAX_FRAMES_IN_FLIGHT * bindingInfo.layoutBinding.descriptorCount
            );
        }
        mUniformPool = poolBuilder.Build();

        for (const auto& bindingInfo : settings.bindings)
        {
            if (bindingInfo.layoutBinding.stageFlags & (VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                VK_SHADER_STAGE_MISS_BIT_KHR |
                VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
                VK_SHADER_STAGE_CALLABLE_BIT_KHR))
            {
                rayTracingBindings.push_back(bindingInfo.layoutBinding);
            }
            else
            {
                rasterBindings.push_back(bindingInfo.layoutBinding);
            }

            std::vector<Buffer> buffers;

            for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
            {
                if (bindingInfo.layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                    bindingInfo.layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                    bindingInfo.layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                {
                    continue; // No buffer needed
                }

                Buffer buffer{};

                // Ensure correct buffer usage (uniform or storage)
                VkBufferUsageFlags2KHR usage = static_cast<VkBufferUsageFlags2KHR>(
                    bindingInfo.bufferUsage | VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR
                );

                Allocator::CreateBuffer(
                    buffer,
                    bindingInfo.elementSize * bindingInfo.elementCount,
                    usage,
                    VMA_MEMORY_USAGE_AUTO,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
                );

                std::string dbgName = bindingInfo.debugName + std::to_string(frameIndex);
                Allocator::SetAllocationName(buffer.allocation, dbgName.c_str());

                buffers.push_back(std::move(buffer));

                if (!bindingInfo.doubleBuffered) break;
            }

            mBuffersPerBinding[bindingInfo.layoutBinding.binding] = std::move(buffers);
        }

        DescriptorSetLayout::Builder layoutBuilder(device);
        for (const auto& bindingInfo : settings.bindings)
        {
            layoutBuilder.AddBinding(bindingInfo.layoutBinding);
        }
        mUniformDescriptorLayout = layoutBuilder.Build();

        settings.Init(*this, renderData);
    }

    void Uniform::Bind(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, int frameIndex, VkPipelineBindPoint bindPoint)
    {
        vkCmdBindDescriptorSets(
            commandBuffer,
            bindPoint,
            pipelineLayout,
            mPipelineBindingIndex,
            1,
            &mUniformDescriptorSets[frameIndex],
            0,
            nullptr);
    }

    Uniform::~Uniform()
    {
        for (auto& [binding, buffers] : mBuffersPerBinding)
        {
            for (auto& buffer : buffers)
            {
                if (buffer.buffer)
                {
                    vmaDestroyBuffer(Allocator::GetAllocator(), buffer.buffer, buffer.allocation);
                }
            }
        }
    }
}
