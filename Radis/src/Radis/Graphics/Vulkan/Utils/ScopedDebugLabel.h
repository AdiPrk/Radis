/*****************************************************************//**
 * \file   ScopedDebugLabel.h
 * \brief  Definition of the ScopedDebugLabel class for Vulkan debug labeling.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    class Device;

    class ScopedDebugLabel
    {
    public:
        ScopedDebugLabel(Device* device, VkCommandBuffer commandBuffer, const char* labelName, const glm::vec4& color);
        ~ScopedDebugLabel();
    private:
        Device* mDevice;
        VkCommandBuffer mCmd;
    };
}
