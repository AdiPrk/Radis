#pragma once

#include "../Core/Buffer.h"

namespace Dog
{
    class Device;
    class Uniform;

    struct TutoPushConstant
    {
        glm::mat3       normalMatrix;
        int            instanceIndex;              // Instance index for the current draw call
        glm::vec2      metallicRoughnessOverride;  // Metallic and roughness override values
    };

    class RaytracingPipeline
    {
    public:
        RaytracingPipeline(Device& device, const std::vector<Uniform*>& uniforms);
        ~RaytracingPipeline();

        void Destroy();
        void Recreate();
        void CreateShaderBindingTable(const VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);
        void Bind(VkCommandBuffer commandBuffer);

        VkPipeline GetPipeline() const { return mRtPipeline; }
        VkPipelineLayout& GetLayout() { return mRtPipelineLayout; }
        VkStridedDeviceAddressRegionKHR& GetRaygenRegion() { return mRaygenRegion; }
        VkStridedDeviceAddressRegionKHR& GetMissRegion() { return mMissRegion; }
        VkStridedDeviceAddressRegionKHR& GetHitRegion() { return mHitRegion; }
        VkStridedDeviceAddressRegionKHR& GetCallableRegion() { return mCallableRegion; }

    private:
        Device& device;

        Buffer mSbtBuffer;
        VkPipeline mRtPipeline;
        VkPipelineLayout mRtPipelineLayout;
        std::vector<uint8_t> mShaderHandles;     // Storage for shader group handles
        VkStridedDeviceAddressRegionKHR mRaygenRegion{};    // Ray generation shader region
        VkStridedDeviceAddressRegionKHR mMissRegion{};      // Miss shader region
        VkStridedDeviceAddressRegionKHR mHitRegion{};       // Hit shader region
        VkStridedDeviceAddressRegionKHR mCallableRegion{};  // Callable shader region
    };
}
