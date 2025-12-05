/*********************************************************************
 * file:   Uniform.hpp
 * author: aditya.prakash (aditya.prakash@digipen.edu) and evan.gray (evan.gray@digipen.edu)
 * date:   September 24, 2024
 * Copyright © 2024 DigiPen (USA) Corporation.
 *
 * brief:  Handles the discriptor sets and buffers for sending
 *         uniform data to shaders
 *********************************************************************/
#pragma once

#include "../Core/Buffer.h"

namespace Radis
{
    //Forward declarations
    class Device;
    struct UniformSettings;
    struct RenderingResource;
    class DescriptorPool;
    class DescriptorSetLayout;

    class Uniform {
    public:
        /*********************************************************************
         * param:  device: The vulkan device
         * param:  renderData: The rendering resource
         * param:  settings: The settings for the uniform
         *
         * brief:  Constructor for the uniform
         *********************************************************************/
        Uniform(Device& device, RenderingResource& renderData, const UniformSettings& settings);
        ~Uniform();

        /*********************************************************************
         * param:  commandBuffer: The command buffer
         * param:  pipelineLayout: The pipeline layout
         * param:  frameIndex: The frame index
         * param:  bindPoint: The pipeline stage to bind to
         *
         * brief:  Binds the uniform to the command buffer
         *********************************************************************/
        void Bind(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, int frameIndex, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

        /*********************************************************************
         * brief:  Sets the uniform's data
         *********************************************************************/
        template<typename T>
        void SetUniformData(const T& data, int bindingIndex, int frameIndex);

        template<typename T>
        void SetUniformData(const std::vector<T>& data, int bindingIndex, int frameIndex);

        template<typename T>
        void SetUniformData(const std::vector<T>& data, int bindingIndex, int frameIndex, int count);

        template<typename T, std::size_t N>
        void SetUniformData(const std::array<T, N>& data, int bindingIndex, int frameIndex);

        /*********************************************************************
         * param:  binding: The descriptor set's binding index
         *
         * brief:  Sets the descriptor set's binding index
         *********************************************************************/
        void SetBinding(unsigned int binding) { mPipelineBindingIndex = binding; }

        // Getters
        std::unique_ptr<DescriptorSetLayout>& GetDescriptorLayout() { return mUniformDescriptorLayout; }
        std::vector<VkDescriptorSet>& GetDescriptorSets() { return mUniformDescriptorSets; }
        Buffer& GetUniformBuffer(int binding, int frameIndex) { return mBuffersPerBinding[binding][frameIndex]; }
        std::unique_ptr<DescriptorPool>& GetDescriptorPool() { return mUniformPool; }

        std::vector<VkDescriptorSetLayoutBinding>& GetRasterBindings() { return rasterBindings; }
        std::vector<VkDescriptorSetLayoutBinding>& GetRayTracingBindings() { return rayTracingBindings; }

    private:
        std::unordered_map<int, std::vector<Buffer>> mBuffersPerBinding;
        std::vector<VkDescriptorSet> mUniformDescriptorSets;
        std::unique_ptr<DescriptorPool> mUniformPool;
        std::unique_ptr<DescriptorSetLayout> mUniformDescriptorLayout;
        unsigned int mPipelineBindingIndex = std::numeric_limits<unsigned int>::max();
        Device& mDevice;

        std::vector<VkDescriptorSetLayoutBinding> rasterBindings;
        std::vector<VkDescriptorSetLayoutBinding> rayTracingBindings;
    };

} // namespace Radis

#include "Uniform.inl"