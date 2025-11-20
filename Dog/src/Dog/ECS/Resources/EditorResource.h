#pragma once

#include "IResource.h"
#include "ECS/Entities/Entity.h"

namespace Dog
{
    class Device;
    class SwapChain;

    struct EditorResource : public IResource
    {
        EditorResource(Device* device, SwapChain* swapChain, GLFWwindow* glfwWindow, float dpiScale);
        EditorResource(GLFWwindow* glfwWindow, float dpiScale);

        void Create(Device* device, SwapChain* swapChain, GLFWwindow* glfwWindow, float dpiScale);
        void Create(GLFWwindow* glfwWindow, float dpiScale);
        void Cleanup(Device* device);

        void SetupFonts(float dpiScale);

        VkDescriptorPool descriptorPool;
        VkDescriptorSetLayout samplerSetLayout;

        float sceneWindowX = 1.f;
        float sceneWindowY = 1.f;
        float sceneWindowWidth = 1.f;
        float sceneWindowHeight = 1.f;

        Entity selectedEntity;
        Entity entityToDelete;

        bool GetImGuiInitialized() { return isInitialized; }

    private:
        bool isInitialized = false;
    };
}
