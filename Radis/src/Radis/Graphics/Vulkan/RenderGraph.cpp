/*****************************************************************//**
 * \file   RenderGraph.cpp
 * \brief  Implementation of the RenderGraph class for managing render passes and resources.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "RenderGraph.h"
#include "Graphics/Vulkan/Texture/VKTexture.h"

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

    void RGPassBuilder::setCompute()
    {
        m_pass.type = PassType::Compute;
    }

    void RGPassBuilder::setRaytrace()
    {
        m_pass.type = PassType::Raytrace;
    }

    RGResourceHandle RenderGraph::ImportTexture(const char* name, VKTexture* tex, bool backBuffer)
    {
        if (!tex)
        {
            RADIS_CRITICAL("RenderGraph::ImportTexture: VKTexture pointer is null for resource '{}'", name);
            return { UINT32_MAX };
        }

        return ImportTexture(name, tex->GetImage(), tex->GetImageView(), tex->GetExtent(), tex->GetImageFormat(), backBuffer);
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

    RGResourceHandle RenderGraph::ImportBackbuffer(const char* name, VKTexture* tex)
    {
        return ImportTexture(name, tex, true);
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

    // Helper to check if format is a depth format
    static bool IsDepthFormat(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT ||
               format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
               format == VK_FORMAT_D24_UNORM_S8_UINT ||
               format == VK_FORMAT_D16_UNORM ||
               format == VK_FORMAT_D16_UNORM_S8_UINT;
    }

    void RenderGraph::Execute(VkCommandBuffer cmd, VkDevice device)
    {
        for (const auto& pass : mPasses)
        {
            const bool isCompute = (pass.type == PassType::Compute);
            const bool isRaytrace = (pass.type == PassType::Raytrace);

            // ---------------------------------------------------------------
            // Barriers for READ targets
            // - Graphics: transition to SHADER_READ_ONLY_OPTIMAL, dst stage = FRAGMENT_SHADER
            // - Compute reads: transition to SHADER_READ_ONLY_OPTIMAL, dst stage = COMPUTE_SHADER
            // ---------------------------------------------------------------
            for (const auto& handleName : pass.readTargets)
            {
                RGResourceHandle handle = GetResourceHandle(handleName);
                if (handle.index == UINT32_MAX) continue;

                RGResource& resource = mResources[handle.index];
                bool isDepth = IsDepthFormat(resource.format);

                VkImageLayout targetLayout = isDepth
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                if (resource.currentLayout != targetLayout)
                {
                    VkImageMemoryBarrier barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.oldLayout = resource.currentLayout;
                    barrier.newLayout = targetLayout;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.image = resource.image;
                    barrier.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    barrier.subresourceRange.baseArrayLayer = 0;
                    barrier.subresourceRange.layerCount = 1;

                    // src: whatever wrote this resource last frame/pass
                    barrier.srcAccessMask = resource.lastWriteAccess;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    VkPipelineStageFlags dstStage = isCompute
                        ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                        : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

                    vkCmdPipelineBarrier(cmd, resource.lastWriteStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

                    resource.currentLayout = targetLayout;
                }
            }

            // ---------------------------------------------------------------
            // Barriers for WRITE targets
            // - Graphics: COLOR/DEPTH_ATTACHMENT_OPTIMAL
            // - Compute: GENERAL
            // ---------------------------------------------------------------
            std::unordered_set<RGResource*> previouslyWrittenResources;

            for (const auto& handleName : pass.writeTargets)
            {
                RGResourceHandle handle = GetResourceHandle(handleName);
                if (handle.index == UINT32_MAX) continue;

                RGResource& resource = mResources[handle.index];
                bool isDepth = IsDepthFormat(resource.format);

                VkImageLayout newLayout;
                VkAccessFlags dstAccess;
                VkPipelineStageFlags dstStage;

                if (isCompute)
                {
                    newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
                    dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                }
                else if (isDepth)
                {
                    newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                }
                else
                {
                    newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                }

                if (resource.currentLayout == newLayout)
                {
                    previouslyWrittenResources.insert(&resource);
                }

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
                    barrier.srcAccessMask = resource.lastWriteAccess;
                    barrier.dstAccessMask = dstAccess;

                    vkCmdPipelineBarrier(cmd,
                        resource.lastWriteStage, dstStage,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

                    resource.currentLayout = newLayout;
                }

                // Record that we are the new writer of this resource
                resource.lastWriteStage = dstStage;
                resource.lastWriteAccess = dstAccess;
            }

            // ---------------------------------------------------------------
            // Execute the pass
            // ---------------------------------------------------------------
            if (isCompute || isRaytrace)
            {
                // No render pass, just dispatch
                pass.executeCallback(cmd);
            }
            else if (!pass.writeTargets.empty())
            {
                // Collect color and depth targets
                std::vector<RGResource*> colorTargets;
                RGResource* depthTarget = nullptr;

                for (const auto& handleName : pass.writeTargets)
                {
                    RGResourceHandle handle = GetResourceHandle(handleName);
                    if (handle.index == UINT32_MAX) continue;

                    RGResource& res = mResources[handle.index];
                    if (IsDepthFormat(res.format))
                        depthTarget = &res;
                    else
                        colorTargets.push_back(&res);
                }

                if (!colorTargets.empty())
                {
                    std::vector<VkRenderingAttachmentInfo> colorAttachments(colorTargets.size());
                    for (size_t i = 0; i < colorTargets.size(); ++i)
                    {
                        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
                        colorAttachments[i].pNext = nullptr;
                        colorAttachments[i].imageView = colorTargets[i]->imageView;
                        colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        colorAttachments[i].resolveMode = VK_RESOLVE_MODE_NONE;
                        colorAttachments[i].resolveImageView = VK_NULL_HANDLE;
                        colorAttachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                        colorAttachments[i].loadOp = previouslyWrittenResources.count(colorTargets[i]) > 0
                            ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
                        colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                        colorAttachments[i].clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
                        //colorAttachments[i].clearValue.color = { {1.0f, 1.0f, 1.0f, 1.0f} }; // for moments

                    }

                    VkRenderingAttachmentInfo depthAttachment{};
                    if (depthTarget)
                    {
                        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
                        depthAttachment.pNext = nullptr;
                        depthAttachment.imageView = depthTarget->imageView;
                        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
                        depthAttachment.resolveImageView = VK_NULL_HANDLE;
                        depthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                        depthAttachment.loadOp = previouslyWrittenResources.count(depthTarget) > 0
                            ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
                        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
                    }

                    VkRenderingInfo renderingInfo{};
                    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
                    renderingInfo.pNext = nullptr;
                    renderingInfo.flags = 0;
                    renderingInfo.renderArea = { {0, 0}, colorTargets[0]->extent };
                    renderingInfo.layerCount = 1;
                    renderingInfo.viewMask = 0;
                    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
                    renderingInfo.pColorAttachments = colorAttachments.data();
                    renderingInfo.pDepthAttachment = depthTarget ? &depthAttachment : nullptr;
                    renderingInfo.pStencilAttachment = nullptr;

                    vkCmdBeginRendering(cmd, &renderingInfo);
                    pass.executeCallback(cmd);
                    vkCmdEndRendering(cmd);
                }
                else
                {
                    // No color targets, don't start a render pass
                    pass.executeCallback(cmd);
                }
            }
            else
            {
                // No write targets, don't start a render pass
                pass.executeCallback(cmd);
            }
        }

        // ---------------------------------------------------------------
        // Final transition for presentation if back buffer
        // ---------------------------------------------------------------
        for (auto& resource : mResources)
        {
            if (!resource.isBackBuffer) continue;
            if (resource.currentLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) continue;

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = resource.currentLayout;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = resource.image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = resource.lastWriteAccess;
            barrier.dstAccessMask = 0;

            vkCmdPipelineBarrier(cmd, resource.lastWriteStage,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            resource.currentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
        }
    }

    void RenderGraph::Clear()
    {
        mPasses.clear();
        mResources.clear();
        mResourceLookup.clear();
    }

    void RenderGraph::Resize(uint32_t width, uint32_t height)
    {
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