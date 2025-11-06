#pragma once

namespace Dog
{
    class Uniform;
    struct RenderingResource;

    struct UniformBindingInfo
    {
        VkDescriptorSetLayoutBinding layoutBinding;  // Layout info for this binding
        VkBufferUsageFlags bufferUsage;              // Buffer usage (e.g., VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        size_t elementSize;                          // Size of each element in this binding
        size_t elementCount;                         // Number of elements for dynamic buffers (SSBOs)
        bool buffered = true;                        // True if this binding is stored in a buffer
        bool doubleBuffered = true;
    };

    struct UniformSettings
    {
        std::vector<UniformBindingInfo> bindings;  // List of binding configurations
        void (*Init)(Uniform&, RenderingResource&);
        uint32_t nextBinding = 0;  // Tracks the next available binding slot

        explicit UniformSettings(void (*initFunc)(Uniform&, RenderingResource&))
            : Init(initFunc)
        {
        }

        // Add a uniform buffer binding
        UniformSettings& AddUBBinding(VkShaderStageFlags stageFlags, size_t elementSize, size_t elementCount = 1)
        {
            bindings.push_back({ { nextBinding++, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, stageFlags }, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, elementSize, elementCount });
            return *this;
        }

        // Add a storage buffer binding
        UniformSettings& AddSSBOBinding(VkShaderStageFlags stageFlags, size_t elementSize, size_t elementCount, bool buffered = true, bool doubleBuffered = true)
        {
            bindings.push_back({ { nextBinding++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, stageFlags }, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, elementSize, elementCount, buffered, doubleBuffered });
            return *this;
        }

        UniformSettings& AddSSBOIndirectBinding(VkShaderStageFlags stageFlags, size_t elementSize, size_t elementCount, bool buffered = true, bool doubleBuffered = true)
        {
            bindings.push_back({ { nextBinding++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, stageFlags }, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, elementSize, elementCount, buffered, doubleBuffered });
            return *this;
        }

        UniformSettings& AddVertexBinding(VkShaderStageFlags stageFlags, size_t elementSize, size_t elementCount, bool buffered = true, bool doubleBuffered = true)
        {
            bindings.push_back({ { nextBinding++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, stageFlags }, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, elementSize, elementCount, buffered, doubleBuffered });
            return *this;
        }

        // Add an image sampler binding
        UniformSettings& AddISBinding(VkShaderStageFlags stageFlags, uint32_t descriptorCount)
        {
            bindings.push_back({ { nextBinding++, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount, stageFlags }, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0, descriptorCount, false });
            return *this;
        }

        UniformSettings& AddSSBIBinding(VkShaderStageFlags stageFlags, uint32_t descriptorCount)
        {
            bindings.push_back({ { nextBinding++, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorCount, stageFlags }, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, descriptorCount, false });
            return *this;
        }

        UniformSettings& AddASBinding(VkShaderStageFlags stageFlags, uint32_t descriptorCount = 1)
        {
            bindings.push_back({ { nextBinding++, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, descriptorCount, stageFlags }, 0, 0, descriptorCount, false, false });
            return *this;
        }
    };
}
