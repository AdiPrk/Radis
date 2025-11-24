#pragma once

namespace Dog
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
