/*****************************************************************//**
 * \file   EditorResource.h
 * \brief  Resource for editor-specific data and settings
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "IResource.h"
#include "ECS/Entities/Entity.h"

namespace Radis
{
    class Device;
    class SwapChain;

    struct EditorResource : public IResource
    {
        EditorResource(Device* device, SwapChain* swapChain, GLFWwindow* glfwWindow, float dpiScale);

        void Create(Device* device, SwapChain* swapChain, GLFWwindow* glfwWindow, float dpiScale);
        void Cleanup(Device* device);

        void SetupFonts(float dpiScale);

        VkDescriptorPool descriptorPool;
        VkDescriptorSetLayout samplerSetLayout;

        float sceneWindowX = 1.f;
        float sceneWindowY = 1.f;
        float sceneWindowWidth = 1.f;
        float sceneWindowHeight = 1.f;
        bool renderRaytracingHeatmap = false;

        std::vector<Entity> selectedEntities;
        std::vector<Entity> entitiesToDelete;

        // Entity selectedEntity;
        // Entity entityToDelete;

        bool GetImGuiInitialized() { return isInitialized; }

    private:
        bool isInitialized = false;
    };
}
