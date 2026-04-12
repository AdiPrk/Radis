/*****************************************************************//**
 * \file   PresentSystem.cpp
 * \brief  Handles presentation to the screen
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "PresentSystem.h"
#include "ECS/ECS.h"

#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/WindowResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Resources/DebugDrawResource.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Core/Synchronization.h"
#include "Graphics/Vulkan/VulkanWindow.h"
#include "Graphics/Vulkan/Uniform/UniformData.h"

#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/Vulkan/Texture/VKTexture.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"

#include "Engine.h"

namespace Radis
{
    // Helper function to resize all textures and update descriptors
    static void ResizeRenderTargets(RenderingResource* rr)
    {
        auto tl = rr->textureLibrary.get();
        if (!tl) return;

        const auto& extent = rr->swapChain->GetSwapChainExtent();
        tl->ResizeStorageImage("RTAccum_0", extent.width, extent.height);
        tl->ResizeStorageImage("RTAccum_1", extent.width, extent.height);
        tl->ResizeStorageImage("RTHeatmapImage_0", extent.width, extent.height);
        tl->ResizeStorageImage("RTHeatmapImage_1", extent.width, extent.height);
        tl->ResizeTexture("SceneTexture", extent.width, extent.height);
        tl->ResizeTexture("SceneDepth", extent.width, extent.height);
        tl->ResizeTexture("SceneHDR", extent.width, extent.height);
        tl->ResizeTexture("gAlbedo", extent.width, extent.height);
        tl->ResizeTexture("gNormal", extent.width, extent.height);
        tl->ResizeTexture("gPBR", extent.width, extent.height);
        tl->ResizeTexture("gEmissive", extent.width, extent.height);
        tl->ResizeTexture("RawAO", extent.width, extent.height);
        tl->ResizeTexture("AOBlurTmp", extent.width, extent.height);
        tl->ResizeTexture("BlurredAO", extent.width, extent.height);

        if (rr->deferredLightingUniform) rr->deferredLightingUniform->UpdateDescriptorSets(*rr);
        if (rr->tonemapUniform) rr->tonemapUniform->UpdateDescriptorSets(*rr);
        
        if (rr->alchemyAOUniform)
        {
            rr->alchemyAOUniform->UpdateDescriptorSets(*rr);
            rr->aoBlurHUniform->UpdateDescriptorSets(*rr);
            rr->aoBlurVUniform->UpdateDescriptorSets(*rr);
        }
    }

	void PresentSystem::Init()
	{
        //if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        //{
        //    auto rr = ecs->GetResource<RenderingResource>();
        //    ResizeRenderTargets(rr);
        //}
	}

	void PresentSystem::FrameStart()
	{
        DebugDrawResource::Clear();

        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            return;
        }

        auto rr = ecs->GetResource<RenderingResource>();
        auto wr = ecs->GetResource<WindowResource>();
        auto& rg = rr->renderGraph;

        // Wait on the current frame's fence (ensures the previous frame's GPU work is done).
        rr->syncObjects->WaitForCommandBuffers();

        // Aquire next image from swapchain
        VkResult result = rr->swapChain->AcquireNextImage(&rr->currentImageIndex, *rr->syncObjects);

        // Recreate if needed
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            rr->RecreateSwapChain(wr->window.get());
            ResizeRenderTargets(rr);
            return;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            RADIS_CRITICAL("Failed to present swap chain image");
        }

        // If image is in flight, wait for its fence
        if (rr->syncObjects->GetImageInFlightFence(rr->currentImageIndex) != VK_NULL_HANDLE) {
            vkWaitForFences(rr->device->GetDevice(), 1, &rr->syncObjects->GetImageInFlightFence(rr->currentImageIndex), VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now in use by this frame:
        rr->syncObjects->GetImageInFlightFence(rr->currentFrameIndex) = rr->syncObjects->GetCommandBufferInFlightFence();


        // Get the command buffer for the current frame and reset it
        VkCommandBuffer commandBuffer = rr->commandBuffers[rr->currentFrameIndex];
        vkResetCommandBuffer(commandBuffer, 0);

        // --- Begin Recording with Render Graph ---
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // Start a new render graph!
        rg->Clear();

        // Import resources!
        auto tl = rr->textureLibrary.get();

        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            std::string accumName = "RTAccum_" + std::to_string(i);
            std::string heatmapName = "RTHeatmapImage_" + std::to_string(i);

            rg->ImportTexture(accumName.c_str(), (VKTexture*)tl->GetTexture(accumName));
            rg->ImportTexture(heatmapName.c_str(), (VKTexture*)tl->GetTexture(heatmapName));
        }

        rg->ImportTexture("SceneTexture", (VKTexture*)tl->GetTexture("SceneTexture"));
        rg->ImportTexture("SceneDepth", (VKTexture*)tl->GetTexture("SceneDepth"));
        rg->ImportTexture("SceneHDR", (VKTexture*)tl->GetTexture("SceneHDR"));
        rg->ImportTexture("gAlbedo", (VKTexture*)tl->GetTexture("gAlbedo"));
        rg->ImportTexture("gNormal", (VKTexture*)tl->GetTexture("gNormal"));
        rg->ImportTexture("gPBR", (VKTexture*)tl->GetTexture("gPBR"));
        rg->ImportTexture("gEmissive", (VKTexture*)tl->GetTexture("gEmissive"));
        rg->ImportTexture("RawAO", (VKTexture*)tl->GetTexture("RawAO"));
        rg->ImportTexture("AOBlurTmp", (VKTexture*)tl->GetTexture("AOBlurTmp"));
        rg->ImportTexture("BlurredAO", (VKTexture*)tl->GetTexture("BlurredAO"));
        
        rg->ImportBackbuffer(
            "BackBuffer",
            rr->swapChain->GetImage(),
            rr->swapChain->GetImageView(),
            rr->swapChain->GetSwapChainExtent(),
            rr->swapChain->GetImageFormat()
        );
	}

	void PresentSystem::Update(float dt)
	{
	}

	void PresentSystem::FrameEnd()
	{
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            return;
        }

        auto rr = ecs->GetResource<RenderingResource>();

        if (!rr)
        {
            RADIS_CRITICAL("RenderingResource not found in PresentSystem");
            return;
        }

        VkCommandBuffer commandBuffer = rr->commandBuffers[rr->currentFrameIndex];

        // Execute the graph
        rr->renderGraph->Execute(commandBuffer, rr->device->GetDevice());

        // --- End Graph ---
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

        // --- Submit the Command Buffer ---
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { rr->syncObjects->GetImageAvailableSemaphore() };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkSemaphore signalSemaphores[] = { rr->syncObjects->GetRenderFinishedSemaphore(rr->currentImageIndex) };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(rr->device->GetDevice(), 1, &rr->syncObjects->GetCommandBufferInFlightFence());
        VkResult result = vkQueueSubmit(rr->device->GetGraphicsQueue(), 1, &submitInfo, rr->syncObjects->GetCommandBufferInFlightFence());
        if (result != VK_SUCCESS)
        {
            RADIS_CRITICAL("Failed to submit draw command buffer!");
            return;
        }

        // --- Presentation ---
        result = rr->swapChain->PresentImage(&rr->currentImageIndex, *rr->syncObjects);

        auto wr = ecs->GetResource<WindowResource>();
        bool winResized = wr->window->WasResized();
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || winResized)
        {
            wr->window->ResetResizeFlag();
            rr->RecreateSwapChain(wr->window.get());
            ResizeRenderTargets(rr);
            rr->syncObjects->ClearImageFences();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        rr->currentFrameIndex = (rr->currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
        rr->syncObjects->NextFrame();
	}

	void PresentSystem::Exit()
	{
	}
}
