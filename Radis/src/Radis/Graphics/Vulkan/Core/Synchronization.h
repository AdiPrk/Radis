#pragma once

namespace Radis
{
    // Forward declaration
    class SwapChain;
    class Device;

    class Synchronizer
    {
    public:
        Synchronizer(VkDevice device, size_t swapChainImageCount);
        ~Synchronizer();

        void WaitForCommandBuffers();
        void ResetCommandBufferFence();

        void ClearImageFences();

        void NextFrame();

        VkSemaphore& GetImageAvailableSemaphore() { return mImageAvailableSemaphores[mCurrentFrame]; }
        VkSemaphore& GetRenderFinishedSemaphore(int imageIndex) { return mRenderFinishedSemaphores[imageIndex]; }
        VkSemaphore& GetComputeFinishedSemaphore() { return mRenderFinishedSemaphores[mCurrentFrame]; }
        VkFence& GetCommandBufferInFlightFence() { return mCommandBuffInFlightFences[mCurrentFrame]; }
        VkFence& GetImageInFlightFence(size_t index) { return mImagesInFlightFences[index]; }

    private:
        /*********************************************************************
         * brief: Creates objects for syncing presenting, iamge fetching, and
         *        submitting command buffers
         *********************************************************************/
        void CreateSyncObjects();

        VkDevice mDevice;
        size_t mSwapChainImageCount;

        std::vector<VkSemaphore> mImageAvailableSemaphores;  // Signaled when images are ready to be used
        std::vector<VkSemaphore> mRenderFinishedSemaphores;  // Signaled when command buffers are finished being submitted
        std::vector<VkSemaphore> mComputeFinishedSemaphores; // Signaled when compute command buffers are finished being submitted
        std::vector<VkFence> mCommandBuffInFlightFences;     // Signaled when command buffer finish execution
        std::vector<VkFence> mImagesInFlightFences;          // Signaled when image is being used

        size_t mCurrentFrame = 0;                           // Current frame being rendered
    };
} // namespace Radis
