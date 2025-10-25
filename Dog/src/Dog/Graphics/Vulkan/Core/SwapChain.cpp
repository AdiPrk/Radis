#include <PCH/pch.h>

#include "SwapChain.h"
#include "Device.h"
#include "Synchronization.h"
#include "ECS/Resources/RenderingResource.h"

namespace Dog
{
    SwapChain::SwapChain(Device& deviceRef, glm::uvec2 extent)
        : mDevice{ deviceRef }
        , mWindowExtent{ extent }
    {
        Init(mDevice.GetInstance());
    }

    SwapChain::SwapChain(Device& deviceRef, glm::uvec2 extent, std::shared_ptr<SwapChain> previous)
        : mDevice{ deviceRef }
        , mWindowExtent{ extent }
        , mOldSwapChain{ previous }
    {
        Init(mDevice.GetInstance());

        //Clean up oldswap chain data since its not used after init
        mOldSwapChain = nullptr;
    }

    SwapChain::~SwapChain()
    {
        //Delete all image views
        for (VkImageView imageView : mSwapChainImageViews)
        {
            vkDestroyImageView(mDevice.GetDevice(), imageView, nullptr);
        }
        mSwapChainImageViews.clear();

        //Delete the swapchain 
        if (mSwapChain != nullptr)
        {
            vkDestroySwapchainKHR(mDevice.GetDevice(), mSwapChain, nullptr);
            mSwapChain = nullptr;
        }

        //Cleanup depth data
        for (size_t i = 0; i < mDepthImages.size(); i++) {
            vkDestroyImageView(mDevice.GetDevice(), mDepthImageViews[i], nullptr);
            vmaDestroyImage(mDevice.GetVmaAllocator(), mDepthImages[i], mDepthImageMemorys[i]);
        }

        //Delete all framebuffers
        for (VkFramebuffer framebuffer : mSwapChainFramebuffers)
        {
            vkDestroyFramebuffer(mDevice.GetDevice(), framebuffer, nullptr);
        }

        //Delete render pass
        vkDestroyRenderPass(mDevice.GetDevice(), mRenderPass, nullptr);
    }

    VkResult SwapChain::AcquireNextImage(uint32_t* imageIndex, Synchronizer& syncObjects)
    {
        //Wait for fence of current frame to have completed (Waits in tell command buffer has completed)
        //vkWaitForFences(mDevice.GetDevice(), 1, &mCommandBuffInFlightFences[mCurrentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

        //Get next image to draw too in the swap chain
        VkResult result = vkAcquireNextImageKHR(mDevice.GetDevice(), //Device swap chain is on
            mSwapChain,                                                //Swapchain being used
            std::numeric_limits<uint64_t>::max(),                      //Use max so no timeout
            syncObjects.GetImageAvailableSemaphore(),              //Give semaphore to be triggered when image is ready for rendering (must be a not signaled semaphore)
            VK_NULL_HANDLE,                                            //No fences in use for this
            imageIndex);                                               //Gets set to image index to use

        mCurrentImageIndex = *imageIndex; //Set current image index to the one we just got

        return result; //Return result of image get call
    }

    VkResult SwapChain::PresentImage(uint32_t* imageIndex, Synchronizer& syncObjects)
    {
        //Put the renderFinished semaphore for the current frame to be submitted as the command buffer complete signal semaphore
        VkSemaphore signalSemaphores[] = { syncObjects.GetRenderFinishedSemaphore(*imageIndex) };

        //Set the this swapchain as the swapchain to use
        VkSwapchainKHR swapChains[] = { mSwapChain };

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR; //Define this structure as a present info
        presentInfo.waitSemaphoreCount = 1;                     //Number of wait semaphores being used

        //Submit the signalSemaphores from the graphics queue completion as the wait, this is so
        //it will wait to present onto the screen in tell the command buffer has finished executing on the gpu
        presentInfo.pWaitSemaphores = signalSemaphores;

        presentInfo.swapchainCount = 1;                         //Number of swap chains being used
        presentInfo.pSwapchains = swapChains;                   //Provide swapchain to present to
        presentInfo.pImageIndices = imageIndex;                 //Index of the image to present to

        //Run command buffer present and save the result
        VkResult result = vkQueuePresentKHR(mDevice.GetPresentQueue(), &presentInfo);

        //Return saved result
        return result;
    }

    void SwapChain::Init(VkInstance instance)
    {
        CreateSwapChain(instance);
        CreateImageViews();
        CreateRenderPass();
        CreateDepthResources();
        CreateFramebuffers();
    }

    void SwapChain::CreateSwapChain(VkInstance instance)
    {
        //Get the swap chain support details of the current device
        SwapChainSupportDetails swapChainSupport = mDevice.GetSwapChainSupport();

        //Get format to use for this swapchain's surface
        VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);

        //Get present mode to use for this swapchain
        VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);

        //Gets the extent (size) to use for this swapchain
        VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

        //Set the image count to use to be one more than the minimum
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        //If the max exists and is smaller than current imagecount
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            //Set image count to max image count
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        //Create info for the swapchain 
        VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
        swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; //Set what will be created to a swapchain
        swapChainCreateInfo.surface = mDevice.GetSurface();                     //Set surface to use to be our device's surface

        swapChainCreateInfo.minImageCount = imageCount;                       //Set minumum image count to use as the image count we found
        swapChainCreateInfo.imageFormat = surfaceFormat.format;               //Set format to use
        swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;       //Set color space to use
        swapChainCreateInfo.imageExtent = extent;                             //Set extent (size) to use
        swapChainCreateInfo.imageArrayLayers = 1;                             //Multilayed images used for steroscopic-3D (VR), so just 1 layer for us
        swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //Set what this swapchain will be used for to color attachment (rendering) 

        //Find submission queues of our device ajnd get their indices
        QueueFamilyIndices indices = mDevice.FindPhysicalQueueFamilies();
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

        //Check if the two queues are not the same (if our device does not use one queue for both)
        if (indices.graphicsFamily != indices.presentFamily)
        {
            //Set sharing mode to concurrent so multiple queue families can access this swapchain
            swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;

            //Set number of queue families accessing this and their indices
            swapChainCreateInfo.queueFamilyIndexCount = 2;
            swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            //Set sharing mode to exclusive since it will only be submitted to one queue familiy
            swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

            //Not required with mode exclusive, so just set to defualts
            swapChainCreateInfo.queueFamilyIndexCount = 0;
            swapChainCreateInfo.pQueueFamilyIndices = nullptr;
        }

        //Set preTransform to be applied before presenting to our device's currentTransform,
        //so it will always be transformed to what the device's surface requires (like if it was a tablet and the screen rotated)
        swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        //Set compositeAlpha to opaque, which means when compositing alpha is always treated as 1.0 
        //Basicly just not using alpha currently
        swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        //Set present mode to found present mode
        swapChainCreateInfo.presentMode = presentMode;

        //Dont render to non-visable pixels (like if covered by another window)
        swapChainCreateInfo.clipped = VK_TRUE;

        //Dont provide old swapchain if it doesnt exist, else provide it so it can resuse memory when possible
        swapChainCreateInfo.oldSwapchain = (mOldSwapChain == nullptr) ? VK_NULL_HANDLE : mOldSwapChain->mSwapChain;

        //Create swap chain
        if (vkCreateSwapchainKHR(mDevice.GetDevice(), &swapChainCreateInfo, nullptr, &mSwapChain) != VK_SUCCESS)
        {
            //If failed throw error
            DOG_CRITICAL("Failed to create swap chain");
        }

        //We only specified a minimum number of images in the swap chain, so the implementation is
        //allowed to create a swap chain with more. That's why we'll first query the final number of
        //images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
        //retrieve the handles.
        vkGetSwapchainImagesKHR(mDevice.GetDevice(), mSwapChain, &imageCount, nullptr);
        mSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(mDevice.GetDevice(), mSwapChain, &imageCount, mSwapChainImages.data());

        //Set format and extent member varibles
        mSwapChainImageFormat = surfaceFormat.format;
        mSwapChainExtent = extent;
    }

    void SwapChain::CreateImageViews()
    {
        //Resize image views to fit a view for all images in swapChainImages
        mSwapChainImageViews.resize(mSwapChainImages.size());

        //Go through all images
        for (size_t i = 0; i < mSwapChainImages.size(); i++)
        {
            //Create info for a image view, which is basicly a descriptor for an image (stuff like 2d or 3d, layers, ect.)
            VkImageViewCreateInfo viewCreateInfo{};
            viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;        //Set what will be created to a image view
            viewCreateInfo.image = mSwapChainImages[i];                             //Set the image this view is referring to
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;                        //Set so this image is 2D
            viewCreateInfo.format = mSwapChainImageFormat;                          //Set the format to saved format found earlier
            viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //Specifiy that the color compontent is what is to be rendered (compared to the depth bit, or something)
            viewCreateInfo.subresourceRange.baseMipLevel = 0;                       //Dont scale down the image by defualt
            viewCreateInfo.subresourceRange.levelCount = 1;                         //Only allow defualt scale level (no other mip levels)
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;                     //Make so the base accessed level into the image is 0
            viewCreateInfo.subresourceRange.layerCount = 1;                         //Make so the image only has one level

            //Create the image view
            if (vkCreateImageView(mDevice.GetDevice(), &viewCreateInfo, nullptr, &mSwapChainImageViews[i]) != VK_SUCCESS)
            {
                DOG_CRITICAL("Failed to create texture image view");
            }
        }
    }

    void SwapChain::CreateRenderPass()
    {
        //Description of depth attachment
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = FindDepthFormat();                                     //Find and set image format to use for this depth buffer
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                                //One sample per pixel
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                           //Tells the depthbuffer attachment to clear each time it is loaded
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;                     //Specifies the contents within the depth buffer render area are not needed after rendering
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                //Specifies that the previous contents within the stencil need not be preserved and will be undefined
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;              //Specifies the contents within the stencil render area are not needed after rendering
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                      //Inital image contents does not matter since we are clearing them on load
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; //Set so the layout after rendering allows read and write access as a depth/stencil attachment

        //Defines the attachment index and the layout while rendering for the subpass (given to subpass below)
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;                                            //Index of this attachment in the swapchain
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; //What this attachment is layed out to support

        //Description of color attachment
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = GetSwapChainImageFormat();                //Set image format to match what is already being used by the swapchain
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                   //One sample per pixel (more samples used for multisampling)
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;              //Tells the colorbuffer attachment to clear each time it is loaded
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;            //Specifies the contents generated during the render pass and within the render area are written to memory.
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; //Specifies that the previous contents within the stencil need not be preserved and will be undefined
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   //Specifies the contents within the stencil render area are not needed after rendering
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;         //Inital image contents does not matter since we are clearing them on load
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;     //Layout used for presenting to screen 

        //Defines the attachment index and the layout while rendering for the subpass (given to subpass below)
        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;                                    //Index of this attachment in the swapchain
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //What this attachment is layed out to support

        //A subpass in Vulkan is a phase of rendering that can read from and write to certain framebuffer attachments (color, depth, and stencil buffers)
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; //Define that this subpass is for graphics output (instead of something like a compute shader)
        subpass.colorAttachmentCount = 1;                            //Number of color attachments used
        subpass.pColorAttachments = &colorAttachmentRef;             //Refenece to the attachment index and layout for the color attachment
        subpass.pDepthStencilAttachment = &depthAttachmentRef;       //Refenece to the attachment index and layout for the depth attachment

        //Declare a dependency for the subpass (forces thread sync between source and destination)
        VkSubpassDependency dependency = {};
        //Sets the source of this dependency to be before the subpasses start
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        //No specific memory access is being waited on from the source operation.
        dependency.srcAccessMask = 0;
        //The render pass should wait until all color attachment writes and early fragment (depth) tests outside of the render pass have been completed.
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        //Sets the destination of this dependency to be the first subpass
        dependency.dstSubpass = 0;
        //Subpass 0's color attachment output and early fragment tests will only begin after the external stages (color attachment output and early fragment tests) are complete.
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        //These access masks ensure that the operations described by dstStageMask (color and depth/stencil attachment writes) will wait for the completion of the stages in the srcStageMask.
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        //Put attachments into an array
        std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

        //Create the render pass from information above
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;           //Define this structor as a render pass create info
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); //Number of attachments being used
        renderPassInfo.pAttachments = attachments.data();                           //Array of all attachments
        renderPassInfo.subpassCount = 1;                                            //Number of subpasses in this renderpass
        renderPassInfo.pSubpasses = &subpass;                                       //Reference to subpass being used
        renderPassInfo.dependencyCount = 1;                                         //Number of dependencies in this renderpass
        renderPassInfo.pDependencies = &dependency;                                 //Reference to dependency being used

        //Create render pass
        if (vkCreateRenderPass(mDevice.GetDevice(), &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS)
        {
            DOG_CRITICAL("Failed to create render pass");
        }
    }

    void SwapChain::CreateFramebuffers()
    {
        //Resize vector of framebuffer to match swapchain image count
        mSwapChainFramebuffers.resize(ImageCount());

        //Go through all framebuffers
        for (size_t i = 0; i < ImageCount(); i++)
        {
            //Get color and depth attachment images for current index
            std::array<VkImageView, 2> attachments = { mSwapChainImageViews[i], mDepthImageViews[i] };

            //Get size of swapchain
            VkExtent2D swapChainExtent = GetSwapChainExtent();

            //Create info for framebufffer
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;           //Define type of structure to be a framebuffer create info
            framebufferInfo.renderPass = mRenderPass;                                    //Renderpass to use this framebuffer with
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); //Number of attachments to use
            framebufferInfo.pAttachments = attachments.data();                           //Array of attachments to use
            framebufferInfo.width = swapChainExtent.width;                               //Size to match swapchain size
            framebufferInfo.height = swapChainExtent.height;                             //Size to match swapchain size
            framebufferInfo.layers = 1;                                                  //Only one layer

            //Create framebufffer
            if (vkCreateFramebuffer(mDevice.GetDevice(), &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS)
            {
                DOG_CRITICAL("Failed to create framebuffer");
            }
        }
    }

    void SwapChain::CreateDepthResources()
    {
        //Get the format of the image being used in this swapchain's renderpass's depth attachment
        VkFormat depthFormat = FindDepthFormat();
        mSwapChainDepthFormat = depthFormat;

        //Get the size of the swapchain output
        VkExtent2D swapChainExtent = GetSwapChainExtent();

        //Resize all vectors of depth images/images data to equal the number of images the swapchain is using
        mDepthImages.resize(ImageCount());
        mDepthImageMemorys.resize(ImageCount());
        mDepthImageViews.resize(ImageCount());

        //Go through each depth image and create an image view for it
        for (int i = 0; i < mDepthImages.size(); i++)
        {
            //Create info for an image
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;         //Define the type of this structure to be a image create info
            imageInfo.imageType = VK_IMAGE_TYPE_2D;                        //Set this as a 2d image
            imageInfo.extent.width = swapChainExtent.width;                //Set size to swapchain size
            imageInfo.extent.height = swapChainExtent.height;              //Set size to swapchain size
            imageInfo.extent.depth = 1;                                    //Set depth to only be 1
            imageInfo.mipLevels = 1;                                       //Only one level of detail options
            imageInfo.arrayLayers = 1;                                     //Only has one layer
            imageInfo.format = depthFormat;                                //Set the format to match the format of the depth attachment on the renderpass
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;                    //Set tiling settings to be optimal
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;           //Not initial layout of the image
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; //Set the image to be used as a depth/stencil attachment
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;                     //No multisampling
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;             //Set so image can only be accessed by one command buffer queue family at a time
            imageInfo.flags = 0;                                           //Set no flags

            // log depth format
            // NLE::LOG_ERROR << "Swapchain Depth format: " << depthFormat;

            //Create image and set up matching image memory, use device local bit so the image is created on gpu memory
            mDevice.GetAllocator()->CreateImageWithInfo(
                imageInfo,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                mDepthImages[i],
                mDepthImageMemorys[i]);

            //Create info for an image view
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;        //Define the type of this structure to be and image view create info
            viewInfo.image = mDepthImages[i];                                 //Set this image to access through this view to be the current depth image
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;                        //Set the type of image to view to 2D
            viewInfo.format = depthFormat;                                    //Set the format of the image to view to match the format of the depth attachment on the renderpass
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; //Set the depth bit to be what can be accessed through this view
            viewInfo.subresourceRange.baseMipLevel = 0;                       //Set defialt level of detail to be 0
            viewInfo.subresourceRange.levelCount = 1;                         //Only one level of detail
            viewInfo.subresourceRange.baseArrayLayer = 0;                     //Set defailt array layer to be on to be 0
            viewInfo.subresourceRange.layerCount = 1;                         //Only one array layer

            //Create image view
            if (vkCreateImageView(mDevice.GetDevice(), &viewInfo, nullptr, &mDepthImageViews[i]) != VK_SUCCESS)
            {
                DOG_CRITICAL("Failed to create texture image view");
            }
        }
    }

    VkSurfaceFormatKHR SwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        //Go through all available surface formats
        for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
        {
            //If this format is a standard RGB 8 bit format and is a nonlinear RGB color space
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                //Use this format
                return availableFormat;
            }
        }

        //If wanted format not found use first availble format
        return availableFormats[0];
    }

    VkPresentModeKHR SwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        //Go through all available present mods
        for (const VkPresentModeKHR& availablePresentMode : availablePresentModes)
        {
            //If this present mode is mailbox
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                //Use this present mode and print out what present mode we are using
                DOG_INFO("Present mode: Mailbox");
                return availablePresentMode;
            }
        }

        ////Go through all available present mods
        //for (const auto &availablePresentMode : availablePresentModes) 
        // {
        //  //If this present mode is immediate
        //  if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) 
        // {
        //    //Use this present mode and print out what present mode we are using
        //    std::cout << "Present mode: Immediate" << std::endl;
        //    return availablePresentMode;
        //  }
        //}

        //If cant find wanted present mode use V-sync and print what we are using

        DOG_INFO("Present mode: V-Sync");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        //If width in passed capabilities exists
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            //Return the extents in passed capabilities
            return capabilities.currentExtent;
        }
        else
        {
            //Get size from window size and min/max of image capabilities
            VkExtent2D actualExtent = { mWindowExtent.x, mWindowExtent.y };
            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
            return actualExtent;
        }
    }

    VkFormat SwapChain::FindDepthFormat()
    {
        //Get image format that supports being used as a depth buffer on device being used
        return mDevice.FindSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }
}
