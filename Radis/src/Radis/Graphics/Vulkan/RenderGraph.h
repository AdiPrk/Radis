/*****************************************************************//**
 * \file   RenderGraph.h
 * \brief  Definition of a simple Render Graph system for managing render passes and resources.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    // Opaque handle to a resource managed by the render graph.
    // This abstracts away the underlying VkImage/VkBuffer.
    struct RGResourceHandle {
        uint32_t index;
        bool operator==(const RGResourceHandle& other) const { return index == other.index; }
        void operator=(const uint32_t& idx) { index = idx; }
    };

    // What kind of GPU work a pass does
    enum class PassType
    {
        Graphics,   // uses vkCmdBeginRendering
        Compute,    // no render pass, writes storage images (GENERAL)
        Raytrace,   // no render pass
    };

    // Internal representation of a resource's state.
    struct RGResource
    {
        std::string name;
        VkImage image{ VK_NULL_HANDLE };
        VkImageView imageView{ VK_NULL_HANDLE };
        VkExtent2D extent;
        VkFormat format;
        bool isBackBuffer;

        // State tracking for automatic barriers
        VkImageLayout currentLayout{ VK_IMAGE_LAYOUT_UNDEFINED };

        // Which pipeline stage last wrote this resource
        VkPipelineStageFlags lastWriteStage{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
        VkAccessFlags        lastWriteAccess{ 0 };
    };

    struct RGPass; // Forward declaration

    // A transient helper object passed to the pass setup lambda.
    // It provides a clean API for declaring what a pass reads from and writes to.
    class RGPassBuilder
    {
    public:
        RGPassBuilder(RGPass& pass) : m_pass(pass) {}

        // Declare that this pass writes to a resource.
        void writes(const std::string& handleName);

        // Declare that this pass reads from a resource.
        void reads(const std::string& handleName);

        // Mark this pass as a compute pass (write targets transition to GENERAL)
        void setCompute();

        // Mark this pass as a ray tracing pass (no render pass, no layout transitions).
        void setRaytrace();

    private:
        RGPass& m_pass;
    };

    // Logical description of a render pass and its resource usage.
    struct RGPass {
        std::string name;
        PassType type{ PassType::Graphics };
        std::function<void(RGPassBuilder&)> setupCallback;
        std::function<void(VkCommandBuffer)> executeCallback;

        std::vector<std::string> writeTargets;
        std::vector<std::string> readTargets;
    };

    // The main orchestrator class
    class RenderGraph {
    public:
        RenderGraph() = default;

        // Imports an existing, externally managed image (like the swapchain) into the graph.
        RGResourceHandle ImportTexture(const char* name, class VKTexture* tex, bool backBuffer = false);
        RGResourceHandle ImportTexture(const char* name, VkImage image, VkImageView view, VkExtent2D extent, VkFormat format, bool backBuffer = false);
        RGResourceHandle ImportBackbuffer(const char* name, class VKTexture* tex);
        RGResourceHandle ImportBackbuffer(const char* name, VkImage image, VkImageView view, VkExtent2D extent, VkFormat format);

        // Adds a new pass to the graph. The setup callback declares dependencies.
        void AddPass(const char* name,
            std::function<void(RGPassBuilder&)>&& setup,
            std::function<void(VkCommandBuffer)>&& execute);

        // Compiles and executes the graph, recording commands into the provided buffer.
        void Execute(VkCommandBuffer cmd, VkDevice device);

        // Clears all passes and resources for the next frame.
        void Clear();

        // Resize
        void Resize(uint32_t width, uint32_t height);


    private:
        RGResourceHandle GetResourceHandle(const std::string& name) const;

        std::vector<RGResource> mResources;
        std::vector<RGPass> mPasses;
        std::unordered_map<std::string, RGResourceHandle> mResourceLookup;
    };
}
