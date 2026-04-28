/*****************************************************************//**
 * \file   ScopedGPUProfiler.h
 * \brief  Wrapper for GPU Profiler
 * 
 * \author Aditya Prakash
 * \date   April 2026
 *********************************************************************/

#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace Radis
{
    class GPUProfiler;

    class ScopedGPUProfiler
    {
    public:
        ScopedGPUProfiler(GPUProfiler* profiler, VkCommandBuffer cmd, const std::string& name);
        ~ScopedGPUProfiler();

    private:
        GPUProfiler* mProfiler;
        VkCommandBuffer mCmd;
        std::string mName;
        uint32_t mStartIndex;
    };
}