#include <PCH/pch.h>
#include "RenderGraph.h"

namespace Radis
{
    void RGPassBuilder::writes(const std::string& handleName)
    {
        m_pass.writeTargets.push_back(handleName);
    }

    void RGPassBuilder::reads(const std::string& handleName)
    {
        m_pass.readTargets.push_back(handleName);
    }

    RGResourceHandle RenderGraph::ImportTexture(const char* name, VkImage image, VkImageView view, VkExtent2D extent, VkFormat format, bool backBuffer)
    {
        RGResource resource;
        resource.name = name;
        resource.image = image;
        resource.imageView = view;
        resource.extent = extent;
        resource.format = format;
        resource.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resource.isBackBuffer = backBuffer;

        mResources.push_back(resource);
        mResourceLookup[name] = static_cast<uint32_t>(mResources.size() - 1);
        return { static_cast<uint32_t>(mResources.size() - 1) };
    }

    RGResourceHandle RenderGraph::ImportBackbuffer(const char* name, VkImage image, VkImageView view, VkExtent2D extent, VkFormat format)
    {
        return ImportTexture(name, image, view, extent, format, true);
    }

    void RenderGraph::AddPass(const char* name,
        std::function<void(RGPassBuilder&)>&& setup,
        std::function<void(VkCommandBuffer)>&& execute)
    {
        RGPass pass;
        pass.name = name;
        pass.setupCallback = std::move(setup);
        pass.executeCallback = std::move(execute);

        RGPassBuilder builder(pass);
        pass.setupCallback(builder);

        mPasses.push_back(pass);
    }

    void RenderGraph::Execute(VkCommandBuffer cmd, VkDevice device)
    {
        for (const auto& pass : mPasses)
        {

            // --- 1. Automatic Barrier Insertion (Image Layout Transitions) ---

            for (const auto& handleName : pass.readTargets)
            {
                RGResourceHandle handle = GetResourceHandle(handleName);
                RGResource& resource = mResources[handle.index];
                if (resource.currentLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                {
                    VkImageMemoryBarrier barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.oldLayout = resource.currentLayout;
                    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.image = resource.image;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.levelCount = 1;
                    barrier.subresourceRange.layerCount = 1;

                    // From color attachment write to shader read
                    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

                    resource.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }


            for (const auto& handleName : pass.writeTargets)
            {
                RGResourceHandle handle = GetResourceHandle(handleName);
                RGResource& resource = mResources[handle.index];

                // Check if it's a depth format
                bool isDepth = (resource.format == VK_FORMAT_D32_SFLOAT);

                VkImageLayout newLayout = isDepth ?
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                // Transition the backbuffer for rendering
                if (resource.currentLayout != newLayout)
                {
                    VkImageMemoryBarrier barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.oldLayout = resource.currentLayout;
                    barrier.newLayout = newLayout;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.image = resource.image;
                    barrier.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    barrier.subresourceRange.baseArrayLayer = 0;
                    barrier.subresourceRange.layerCount = 1;

                    barrier.srcAccessMask = 0; // Or determine from previous usage
                    barrier.dstAccessMask = isDepth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        isDepth ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

                    resource.currentLayout = newLayout;
                }
            }

            // --- 2. Begin Dynamic Rendering ---
            // For simplicity, we assume the first write target is the color attachment.
            // A more robust system would handle multiple attachments.
            if (!pass.writeTargets.empty())
            {
                // Find color and depth targets for the pass
                RGResource* colorTarget = nullptr;
                RGResource* depthTarget = nullptr;
                for (const auto& handleName : pass.writeTargets) {
                    RGResourceHandle handle = GetResourceHandle(handleName);
                    RGResource& res = mResources[handle.index];
                    if (res.format == VK_FORMAT_D32_SFLOAT) { // Or your chosen format
                        depthTarget = &res;
                    }
                    else {
                        colorTarget = &res;
                    }
                }

                if (colorTarget) {

                    VkRenderingAttachmentInfo colorAttachment{};
                    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
                    colorAttachment.imageView = colorTarget->imageView;
                    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                    colorAttachment.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };

                    VkRenderingAttachmentInfo depthAttachment{};
                    if (depthTarget) {
                        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
                        depthAttachment.imageView = depthTarget->imageView;
                        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear depth at start of pass
                        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
                    }

                    VkRenderingInfo renderingInfo{};
                    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
                    renderingInfo.renderArea = { {0, 0}, colorTarget->extent };
                    renderingInfo.layerCount = 1;
                    renderingInfo.colorAttachmentCount = 1;
                    renderingInfo.pColorAttachments = &colorAttachment;
                    renderingInfo.pDepthAttachment = depthTarget ? &depthAttachment : nullptr;
                    renderingInfo.pStencilAttachment = nullptr;

                    vkCmdBeginRendering(cmd, &renderingInfo);

                    pass.executeCallback(cmd);

                    vkCmdEndRendering(cmd);
                }
            }
            else
            {
                // Just execute that
                pass.executeCallback(cmd);
            }
        }

        // --- 3. Final Transition for Presentation ---
        // After all passes, transition the backbuffer for presenting.
        if (!mResources.empty())
        {
            RGResource* finalBackbuffer = nullptr;
            for (auto& resource : mResources)
            {
                if (resource.isBackBuffer)
                {
                    finalBackbuffer = &resource;
                    break;
                }
            }

            if (finalBackbuffer && finalBackbuffer->currentLayout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            {
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = finalBackbuffer->currentLayout;
                barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = finalBackbuffer->image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;

                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = 0;

                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &barrier);

                finalBackbuffer->currentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
        }
    }

    void RenderGraph::Clear()
    {
        mPasses.clear();
        mResources.clear();
    }

    void RenderGraph::Resize(uint32_t width, uint32_t height)
    {
        // resize all resources extents
        for (auto& resource : mResources)
        {
            resource.extent.width = width;
            resource.extent.height = height;
        }
    }

    RGResourceHandle RenderGraph::GetResourceHandle(const std::string& name) const
    {
        auto it = mResourceLookup.find(name);
        if (it != mResourceLookup.end()) {
            return { it->second };
        }

        RADIS_ERROR("Requested resource {0} not found in RenderGraph!", name);
        return { UINT32_MAX };
    }
}