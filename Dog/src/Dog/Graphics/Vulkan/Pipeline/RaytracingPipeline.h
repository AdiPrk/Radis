#pragma once

#include "../Core/Buffer.h"

namespace Dog
{
    class Device;
    class Uniform;

    class RaytracingPipeline
    {
    public:
        RaytracingPipeline(Device& device, const std::vector<Uniform*>& uniforms);
        ~RaytracingPipeline();

        void Destroy();
        void Recreate();
        void CreateShaderBindingTable(const VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);

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
