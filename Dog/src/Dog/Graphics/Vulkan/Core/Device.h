#pragma once

#include "Allocator.h"

namespace Dog {

    class VulkanWindow;

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct QueueFamilyIndices {
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        bool graphicsFamilyHasValue = false;
        bool presentFamilyHasValue = false;
        bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
    };

    class Device {
    public:
#ifdef NDEBUG
        bool enableValidationLayers = false;
#else
        bool enableValidationLayers = true;
#endif

        Device(VulkanWindow& window);
        ~Device();

        // Not copyable or movable
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        operator VkDevice() const { return device_; }

        VkCommandPool GetCommandPool() const { return commandPool; }
        VkDevice GetDevice() const { return device_; }
        VkSurfaceKHR GetSurface() const { return surface_; }
        VkQueue GetGraphicsQueue() const { return graphicsQueue_; }
        VkQueue GetPresentQueue() const { return presentQueue_; }
        const VkPhysicalDevice& GetPhysicalDevice() const { return physicalDevice; }
        const VkInstance& GetInstance() const { return instance; }

        SwapChainSupportDetails GetSwapChainSupport() { return querySwapChainSupport(physicalDevice); }
        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        QueueFamilyIndices FindPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }
        VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        uint32_t GetGraphicsFamily() const { return graphicsFamily_; }
        uint32_t GetPresentFamily() const { return presentFamily_; }

        // Buffer Helper Functions
        
        VkCommandBuffer BeginSingleTimeCommands();
        void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
        void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

        void CreateImageWithInfo(
            const VkImageCreateInfo& imageInfo,
            VmaMemoryUsage memoryUsage,
            VkImage& image,
            VmaAllocation& imageAllocation);

        VkPhysicalDeviceProperties properties;

        void SetFormats(VkFormat srgbFormat, VkFormat linearFormat) { mSrgbFormat = srgbFormat; mLinearFormat = linearFormat; }
        VkFormat GetSrgbFormat() const { return mSrgbFormat; }
        VkFormat GetLinearFormat() const { return mLinearFormat; }

        const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& GetRayTracingProperties() const { return mRtProperties; }
        const VkPhysicalDeviceAccelerationStructurePropertiesKHR& GetAccelerationStructureProperties() const { return mAsProperties; }

        PFN_vkCreateAccelerationStructureKHR g_vkCreateAccelerationStructureKHR;
        PFN_vkCmdBuildAccelerationStructuresKHR g_vkCmdBuildAccelerationStructuresKHR;
        PFN_vkGetAccelerationStructureBuildSizesKHR g_vkGetAccelerationStructureBuildSizesKHR;
        PFN_vkGetAccelerationStructureDeviceAddressKHR g_vkGetAccelerationStructureDeviceAddressKHR;
        PFN_vkDestroyAccelerationStructureKHR g_vkDestroyAccelerationStructureKHR;
        PFN_vkSetDebugUtilsObjectNameEXT g_vkSetDebugUtilsObjectNameEXT;
        PFN_vkCreateRayTracingPipelinesKHR g_vkCreateRayTracingPipelinesKHR;
        PFN_vkGetRayTracingShaderGroupHandlesKHR g_vkGetRayTracingShaderGroupHandlesKHR;
        PFN_vkCmdBeginDebugUtilsLabelEXT g_vkCmdBeginDebugUtilsLabelEXT;
        PFN_vkCmdEndDebugUtilsLabelEXT g_vkCmdEndDebugUtilsLabelEXT;
        PFN_vkCmdTraceRaysKHR g_vkCmdTraceRaysKHR;

        bool SupportsVulkan() const { return mSupportsVulkan; }

    private:
        void createInstance();
        void setupDebugMessenger();
        void createSurface();
        bool pickPhysicalDevice();
        bool createLogicalDevice();
        void createCommandPool();
        void CheckIndirectDrawSupport();

        // helper functions
        bool isDeviceSuitable(VkPhysicalDevice device);
        std::vector<const char*> getRequiredExtensions();
        bool checkValidationLayerSupport();
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        void hasGflwRequiredInstanceExtensions();
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VulkanWindow& window;
        VkCommandPool commandPool;

        VkDevice device_;
        VkSurfaceKHR surface_;
        VkQueue graphicsQueue_;
        VkQueue presentQueue_;

        uint32_t graphicsFamily_ = 0;
        uint32_t presentFamily_ = 0;

        VkFormat mSrgbFormat;
        VkFormat mLinearFormat;

        const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
            VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR mRtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
        VkPhysicalDeviceAccelerationStructurePropertiesKHR mAsProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };

        bool mSupportsVulkan = true;
    };

} // namespace Dog
