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
            std::vector<Buffer> buffers;

            for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
            {
                // No buffer needed for image samplers
                if (bindingInfo.layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                {
                    continue;
                }

                Buffer buffer{};

                // Ensure correct buffer usage (uniform or storage)
                VkBufferUsageFlags2KHR usage = static_cast<VkBufferUsageFlags2KHR>(
                    bindingInfo.bufferUsage | VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR
                );

                device.GetAllocator()->CreateBuffer(
                    buffer,
                    bindingInfo.elementSize * bindingInfo.elementCount,
                    usage,
                    VMA_MEMORY_USAGE_AUTO,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
                );

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
                    vmaDestroyBuffer(mDevice.GetVmaAllocator(), buffer.buffer, buffer.allocation);
                }
            }
        }
    }
}
