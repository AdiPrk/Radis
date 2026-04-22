/*****************************************************************//**
 * \file   GPUProfiler.cpp
 * \brief  Implementation of the gpu profiler
 * 
 * \author Aditya Prakash
 * \date   April 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "GPUProfiler.h"
#include "Graphics/Vulkan/Core/Device.h"

namespace Radis
{
    GPUProfiler::GPUProfiler(Device& device, uint32_t framesInFlight, uint32_t maxProfilesPerFrame)
        : mDevice(device), mMaxQueries(maxProfilesPerFrame * 2), mTimestampPeriod(1)
    {
        if (!mDevice.SupportsTimestamps()) return;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(mDevice.GetPhysicalDevice(), &props);
        mTimestampPeriod = props.limits.timestampPeriod;

        mQueryPools.resize(framesInFlight);
        mFrameProfiles.resize(framesInFlight);

        VkQueryPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = mMaxQueries;

        for (uint32_t i = 0; i < framesInFlight; ++i)
        {
            if (vkCreateQueryPool(mDevice.GetDevice(), &poolInfo, nullptr, &mQueryPools[i]) != VK_SUCCESS)
            {
                RADIS_CRITICAL("Failed to create GPU Query Pool!");
            }
        }
    }

    GPUProfiler::~GPUProfiler()
    {
        if (!mDevice.SupportsTimestamps()) return;

        for (auto pool : mQueryPools)
        {
            if (pool != VK_NULL_HANDLE)
            {
                vkDestroyQueryPool(mDevice.GetDevice(), pool, nullptr);
            }
        }
    }

    void GPUProfiler::BeginFrame(VkCommandBuffer cmd, uint32_t frameIndex)
    {
        if (!mDevice.SupportsTimestamps()) return;

        mCurrentFrameIndex = frameIndex;

        // Retrieve the results from the LAST time this frame index was processed.
        if (!mFrameProfiles[frameIndex].empty() && mCurrentQueryIndex > 0)
        {
            std::vector<uint64_t> rawResults(mCurrentQueryIndex);
            VkResult result = vkGetQueryPoolResults(
                mDevice.GetDevice(),
                mQueryPools[frameIndex],
                0, mCurrentQueryIndex,
                rawResults.size() * sizeof(uint64_t),
                rawResults.data(),
                sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT
            );

            if (result == VK_SUCCESS)
            {
                for (const auto& profile : mFrameProfiles[frameIndex])
                {
                    uint64_t startTick = rawResults[profile.startIndex];
                    uint64_t endTick = rawResults[profile.endIndex];

                    float timeMS = static_cast<float>(endTick - startTick) * mTimestampPeriod / 1000000.0f;
                    mResultsMS[profile.name] = timeMS;
                }
            }
        }

        // Reset the query pool so we can write to it this frame
        vkCmdResetQueryPool(cmd, mQueryPools[frameIndex], 0, mMaxQueries);

        mCurrentQueryIndex = 0;
        mFrameProfiles[frameIndex].clear();
    }

    uint32_t GPUProfiler::WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits stage)
    {
        if (!mDevice.SupportsTimestamps() || mCurrentQueryIndex >= mMaxQueries)
        {
            return 0;
        }

        uint32_t queryIdx = mCurrentQueryIndex++;
        vkCmdWriteTimestamp(cmd, stage, mQueryPools[mCurrentFrameIndex], queryIdx);
        return queryIdx;
    }

    void GPUProfiler::RegisterProfileScope(const std::string& name, uint32_t startIndex, uint32_t endIndex)
    {
        if (!mDevice.SupportsTimestamps() || startIndex >= mMaxQueries || endIndex >= mMaxQueries) return;
        mFrameProfiles[mCurrentFrameIndex].push_back({ name, startIndex, endIndex });
    }
}