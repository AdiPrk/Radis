#include <PCH/pch.h>
#include "ScopedDebugLabel.h"
#include "../Core/Device.h"

namespace Dog
{
    ScopedDebugLabel::ScopedDebugLabel(Device* device, VkCommandBuffer cmd, const char* labelName, const glm::vec4& color)
        : mDevice(device), mCmd(cmd)
    {
        mDevice->StartDebugLabel(cmd, labelName, color);
    }

    ScopedDebugLabel::~ScopedDebugLabel()
    {
        mDevice->EndDebugLabel(mCmd);
    }
}
