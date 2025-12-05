#pragma once

#include <PCH/pch.h>

namespace Radis
{
    class Device;
    struct RenderingResource;
    class Synchronizer;

    class SwapChain
    {
    public:
        //Max number of frames to be executing commands/presenting for at once
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        //Delete copy constructor/operator
        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        /*********************************************************************
         * param:  deviceRef: Device to create this swapchain for
         * param:  windowExtent: Size of the window this swapchain is supporting
         * param:  instance: instance that this swapchan will be created for
         *
         * brief:  Create the swapchain and all other parts required to drawing
         *         a frame
         *********************************************************************/
        SwapChain(Device& deviceRef, glm::uvec2 windowExtent);

        /*********************************************************************
         * param:  deviceRef: Device to create this swapchain for
         * param:  windowExtent: Size of the window this swapchain is supporting
         * param:  previous: Previous swapchain used that is being recreated
         * param:  instance: instance that this swapchan will be created for
         *
         * brief:  Create the swapchain and all other parts required to drawing
         *         a frame, reuse memeory when  possible from old swapchain
         *********************************************************************/
        SwapChain(Device& deviceRef, glm::uvec2 windowExtent, std::shared_ptr<SwapChain> previous);

        /*********************************************************************
         * brief:  Delete all data created by this class
         *********************************************************************/
        ~SwapChain();

        ////////////////////////////////////////////////////////////////////////////////////////////
        /// Getters  ///////////////////////////////////////////////////////////////////////////////

        /*********************************************************************
         * param:  index: Image index to get the framebuffer of
         * return: Framebuffer at wanted index
         *
         * brief:  Gets framebuffer at wanted image index
         *********************************************************************/
        VkFramebuffer GetFrameBuffer(int index) { return mSwapChainFramebuffers[index]; }

        /*********************************************************************
         * return: This swapchain's renderpass
         *
         * brief:  Gets this swapchain's renderpass
         *********************************************************************/
        VkRenderPass GetRenderPass() { return mRenderPass; }

        /*********************************************************************
         * return: The number of images this swapchain uses
         *
         * brief:  Get the number of images this swapchain uses
         *********************************************************************/
        size_t ImageCount() { return mSwapChainImages.size(); }

        /*********************************************************************
         * return: The format of the images this swapchain uses
         *
         * brief:  Get the format of the images this swapchain uses
         *********************************************************************/
        VkFormat GetSwapChainImageFormat() { return mSwapChainImageFormat; }

        /*********************************************************************
         * return: The extent (size) of the images this swapchain uses
         *
         * brief:  Get the extent (size) of the images this swapchain uses
         *********************************************************************/
        VkExtent2D GetSwapChainExtent() { return mSwapChainExtent; }

        /*********************************************************************
         * return: The width of the images this swapchain uses
         *
         * brief:  Gets the width of the images this swapchain uses
         *********************************************************************/
        uint32_t GetWidth() { return mSwapChainExtent.width; }

        /*********************************************************************
         * return: The height of the images this swapchain uses
         *
         * brief:  Gets the height of the images this swapchain uses
         *********************************************************************/
        uint32_t GetHeight() { return mSwapChainExtent.height; }

        /*********************************************************************
         * return: The aspect ratio of the extent of the swapchain
         *
         * brief:  Gets the aspect ratio of the extent of the swapchain
         *********************************************************************/
        float GetExtentAspectRatio() { return static_cast<float>(mSwapChainExtent.width) / static_cast<float>(mSwapChainExtent.height); }

        /*********************************************************************
         * return: Image format to use for depth buffer
         *
         * brief:  Finds image format to use for depth buffers in this swapchain
         *********************************************************************/
        VkFormat FindDepthFormat();

        /*********************************************************************
         * param:  imageIndex: Will be set to next image index
         * return: The result of vkGetNextImage call (error report)
         *
         * brief:  Get the index of the next image to use in the swapchain
         *********************************************************************/
        VkResult AcquireNextImage(uint32_t* imageIndex, Synchronizer& renderData);

        ////////////////////////////////////////////////////////////////////////////////////////////
        /// Helpers ////////////////////////////////////////////////////////////////////////////////

        VkResult PresentImage(uint32_t* imageIndex, Synchronizer& syncObjects);

        /*********************************************************************
         * param:  swapChain: Swapchain to compare againt
         * return: If the swapchain formats match
         *
         * brief:  Compares this swapchain's image and depth formats to
         *         passed swapchain
         *********************************************************************/
        bool CompareSwapFormats(const SwapChain& swapChain) const
        {
            return swapChain.mSwapChainDepthFormat == mSwapChainDepthFormat &&
                swapChain.mSwapChainImageFormat == mSwapChainImageFormat;
        }

        VkFormat GetImageFormat() { return mSwapChainImageFormat; }
        VkFormat GetDepthFormat() { return mSwapChainDepthFormat; }
        VkImage GetImage() { return mSwapChainImages[mCurrentImageIndex]; }
        VkImageView GetImageView() { return mSwapChainImageViews[mCurrentImageIndex]; }
        VkImage GetDepthImage() { return mDepthImages[mCurrentImageIndex]; }
        VkImageView GetDepthImageView() { return mDepthImageViews[mCurrentImageIndex]; }
        uint32_t GetCurrentImageIndex() { return mCurrentImageIndex; }

    private:

        ////////////////////////////////////////////////////////////////////////////////////////////
        /// Helpers ////////////////////////////////////////////////////////////////////////////////

        /*********************************************************************
         * param: instance: Instance to init for
         *
         *
         * brief:  Called in all constructors, required setup for a swapchain
         *         object
         *********************************************************************/
        void Init(VkInstance instance);

        /*********************************************************************
         * param:  instance: instance that this swapchan will be created for
         *
         * brief: Create a swapchain object in vulkan
         *********************************************************************/
        void CreateSwapChain(VkInstance instance);

        /*********************************************************************
         * brief: Creates an image view for each image the swapchain has
         *********************************************************************/
        void CreateImageViews();

        /*********************************************************************
         * brief: Creates all the resoures needed to preform needed depth tests
         *        (Images, image memeory, image views)
         *********************************************************************/
        void CreateDepthResources();

        /*********************************************************************
         * brief: Creates this swapchain's renderpass object
         *********************************************************************/
        void CreateRenderPass();

        /*********************************************************************
         * brief: Creates framebuffers for each image in the swapchain
         *********************************************************************/
        void CreateFramebuffers();

        /*********************************************************************
         * param:  availableFormats: A vector of all available formats that
         *                           our device can use
         * return: The format out of passed vector that should be used
         *
         * brief:  Looks through all available formats and finds which should
         *         be used for this swapchain
         *********************************************************************/
        VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

        /*********************************************************************
         * param:  availablePresentModes: A vector of all available present
         *                                modes that our device can use
         * return: The present mode out of passed vector that should be used
         *
         * brief:  Looks through all available present modes and finds which
         *         should be used for this swapchain
         *********************************************************************/
        VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

        /*********************************************************************
         * param:  capabilities: Capabilities of our device's surface
         * return: The extent (size) that should be used for this swapchain
         *
         * brief:  Using the passed surface capabilities find the extent (size)
         *         that should be used for this swapchain
         *********************************************************************/
        VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

        ////////////////////////////////////////////////////////////////////////////////////////////
        /// Varibles ///////////////////////////////////////////////////////////////////////////////
        /// 
        VkFormat mSwapChainImageFormat; //Format used by this swapchain's images
        VkFormat mSwapChainDepthFormat; //Format used by this swapchain's depthbuffer
        VkExtent2D mSwapChainExtent;    //Extent (size) of this swapchain's images

        std::vector<VkFramebuffer> mSwapChainFramebuffers; //Framebuffer to each swapchain image
        VkRenderPass mRenderPass;                          //Renderpass to be preformed on swapchain images

        std::vector<VkImage> mDepthImages;              //Holds the images that will be used to do depth tests
        std::vector<VmaAllocation> mDepthImageMemorys; //Holds the memory of the depth images
        std::vector<VkImageView> mDepthImageViews;      //Holds the views of the depth images
        std::vector<VkImage> mSwapChainImages;          //Vector of all images this swapchain is using for colors (rendering)
        std::vector<VkImageView> mSwapChainImageViews;  //Vector of all image views for each image, which are descriptors for the images (stuff like if 2d or 3d, how many layers, ect.)

        Device& mDevice;          //Device this swapchain is created for
        glm::uvec2 mWindowExtent; //Extent (size) of the window this swapchain is rendering too

        VkSwapchainKHR mSwapChain;                //Swapchain object in vulkan (Like the whole point of this class)
        std::shared_ptr<SwapChain> mOldSwapChain; //Old swapchain that existed before this one (only exists if this is a recreated swapchain)

        uint32_t mCurrentImageIndex;
    };
}

