#include <PCH/pch.h>
#include "Synchronization.h"

#include "Device.h"
#include "SwapChain.h"
#include "ECS/Resources/RenderingResource.h"

namespace Radis
{
    Synchronizer::Synchronizer(VkDevice device, size_t swapChainImageCount)
        : mDevice(device)
        , mSwapChainImageCount{ swapChainImageCount }
    {
        CreateSyncObjects();
    }

    Synchronizer::~Synchronizer()
    {
        for (size_t i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(mDevice, mComputeFinishedSemaphores[i], nullptr);
            vkDestroyFence(mDevice, mCommandBuffInFlightFences[i], nullptr);
        }

        for (size_t i = 0; i < mSwapChainImageCount; i++)
        {
            vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
        }
    }

    void Synchronizer::CreateSyncObjects()
    {
        mImageAvailableSemaphores.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
        mRenderFinishedSemaphores.resize(mSwapChainImageCount);
        mComputeFinishedSemaphores.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
        mCommandBuffInFlightFences.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
        mImagesInFlightFences.resize(mSwapChainImageCount, VK_NULL_HANDLE);

        //Create info for semaphores, just set type to semaphore
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        //Create info for fences 
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; //Define type of structure to be a fence
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;        //Set to the flag is created signaled (so it doesnt wait for an image to finish rendering first frame when nothing has happened)

        //Go through all frames of sync objects
        for (size_t i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
        {
            //Create two semapgores and a fence
            if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mComputeFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(mDevice, &fenceInfo, nullptr, &mCommandBuffInFlightFences[i]) != VK_SUCCESS)
            {
                DOG_CRITICAL("Failed to create synchronization objects for a frame");
            }
        }

        for (uint32_t i = 0; i < mSwapChainImageCount; i++) {
            if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS)
            {
                DOG_CRITICAL("Failed to create synchronization objects for a frame");
            }
        }
    }
    void Synchronizer::WaitForCommandBuffers()
    {
        VkFence& inFlightFence = GetCommandBufferInFlightFence();
        vkWaitForFences(mDevice, 1, &inFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    void Synchronizer::ResetCommandBufferFence()
    {
        VkFence& inFlightFence = GetCommandBufferInFlightFence();
        vkResetFences(mDevice, 1, &inFlightFence);
    }

    void Synchronizer::ClearImageFences()
    {
        mImagesInFlightFences.clear();
        mImagesInFlightFences.resize(mSwapChainImageCount, VK_NULL_HANDLE);
    }

    void Synchronizer::NextFrame()
    {
        mCurrentFrame = (mCurrentFrame + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
    }

} // namespace Radis
