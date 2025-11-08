#include <PCH/pch.h>
#include "PresentSystem.h"
#include "../ECS.h"

#include "../Resources/RenderingResource.h"
#include "../Resources/WindowResource.h"
#include "../Resources/EditorResource.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Core/Synchronization.h"

#include "Graphics/Vulkan/VulkanWindow.h"

#include "Engine.h"

namespace Dog
{
	void PresentSystem::Init()
	{
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto rr = ecs->GetResource<RenderingResource>();
            rr->RecreateAllSceneTextures();
        }
	}

	void PresentSystem::FrameStart()
	{
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
            rr->RecreateAllSceneTextures();
            return;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            DOG_CRITICAL("Failed to present swap chain image");
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
        if (Engine::GetEditorEnabled()) 
        {
            rg->ImportTexture(
                "SceneColor",
                rr->sceneImage,
                rr->sceneImageView,
                rr->swapChain->GetSwapChainExtent(),
                rr->swapChain->GetImageFormat()
            );
        }

        rg->ImportTexture(
            "SceneDepth",
            rr->mDepthImage,
            rr->mDepthImageView,
            rr->swapChain->GetSwapChainExtent(),
            rr->swapChain->FindDepthFormat()
        );

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
            DOG_CRITICAL("RenderingResource not found in PresentSystem");
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
        if (vkQueueSubmit(rr->device->GetGraphicsQueue(), 1, &submitInfo, rr->syncObjects->GetCommandBufferInFlightFence()) != VK_SUCCESS)
        {
            DOG_CRITICAL("Failed to submit draw command buffer!");
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // --- Presentation ---
        VkResult result = rr->swapChain->PresentImage(&rr->currentImageIndex, *rr->syncObjects);

        auto wr = ecs->GetResource<WindowResource>();
        bool winResized = wr->window->WasResized();
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || winResized)
        {
            wr->window->ResetResizeFlag();
            rr->RecreateSwapChain(wr->window.get());
            rr->RecreateAllSceneTextures();
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
