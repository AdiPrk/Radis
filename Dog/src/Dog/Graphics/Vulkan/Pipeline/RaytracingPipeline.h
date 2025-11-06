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
    };
}
