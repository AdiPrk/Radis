/*****************************************************************//**
 * \file   ScopedGPUProfile.cpp
 * \brief  Implementation of the ScopedGPUProfile RAII wrapper.
 *********************************************************************/

#include <PCH/pch.h>
#include "ScopedGPUProfiler.h"
#include "GPUProfiler.h"

namespace Radis
{
    ScopedGPUProfiler::ScopedGPUProfiler(GPUProfiler* profiler, VkCommandBuffer cmd, const std::string& name)
        : mProfiler(profiler), mCmd(cmd), mName(name), mStartIndex(0)
    {
        if (mProfiler)
        {
            mStartIndex = mProfiler->WriteTimestamp(mCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        }
    }

    ScopedGPUProfiler::~ScopedGPUProfiler()
    {
        if (mProfiler)
        {
            uint32_t endIndex = mProfiler->WriteTimestamp(mCmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
            mProfiler->RegisterProfileScope(mName, mStartIndex, endIndex);
        }
    }
}