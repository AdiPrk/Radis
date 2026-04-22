/*****************************************************************//**
 * \file   GPUProfiler.h
 * \brief  Manages timestamp queries for GPU performance profiling
 * 
 * \author Aditya Prakash
 * \date   April 2026
 *********************************************************************/

#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace Radis
{
    class Device;

    class GPUProfiler
    {
    public:
        GPUProfiler(Device& device, uint32_t framesInFlight, uint32_t maxProfilesPerFrame = 64);
        ~GPUProfiler();

        void BeginFrame(VkCommandBuffer cmd, uint32_t frameIndex);

        uint32_t WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits stage);

        void RegisterProfileScope(const std::string& name, uint32_t startIndex, uint32_t endIndex);

        const std::unordered_map<std::string, float>& GetResults() const { return mResultsMS; }

    private:
        Device& mDevice;
        std::vector<VkQueryPool> mQueryPools;
        uint32_t mMaxQueries;
        float mTimestampPeriod;
        uint32_t mCurrentFrameIndex = 0;
        uint32_t mCurrentQueryIndex = 0;

        struct ProfileData {
            std::string name;
            uint32_t startIndex;
            uint32_t endIndex;
        };

        std::vector<std::vector<ProfileData>> mFrameProfiles;
        std::unordered_map<std::string, float> mResultsMS;
    };
}