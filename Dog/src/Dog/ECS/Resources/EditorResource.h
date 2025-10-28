#pragma once

#include "IResource.h"
#include "ECS/Entities/Entity.h"

namespace Dog
{
    class Device;
    class SwapChain;

    struct EditorResource : public IResource
    {
        EditorResource(Device& device, SwapChain& swapChain, GLFWwindow* glfwWindow);
        EditorResource(GLFWwindow* glfwWindow);

        void Create(Device& device, SwapChain& swapChain, GLFWwindow* glfwWindow);
        void Create(GLFWwindow* glfwWindow);
        void Cleanup(Device* device);

        void CreateSceneTextures(struct RenderingResource* rr);
        void CleanSceneTextures(struct RenderingResource* rr);

        VkDescriptorPool descriptorPool;
        VkDescriptorSetLayout samplerSetLayout;

        float sceneWindowWidth = 1.f;
        float sceneWindowHeight = 1.f;

        Entity selectedEntity;
        Entity entityToDelete;
    };
}
