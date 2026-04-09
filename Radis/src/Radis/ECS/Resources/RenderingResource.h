/*****************************************************************//**
 * \file   RenderingResource.h
 * \brief  The main rendering resource
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "IResource.h"
#include "Graphics/Vulkan/Core/AccelerationStructures.h"
#include "Graphics/OpenGL/GLShader.h"
#include "Graphics/Common/RenderMode.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

namespace Radis
{
    class IWindow;
    class Device;
    class SwapChain;
    class Synchronizer;
    class Pipeline;
    class ComputePipeline;
    class RaytracingPipeline;
    class Renderer;
    class RenderGraph;
    class Uniform;
    class ModelLibrary;
    class TextureLibrary;
    class AnimationLibrary;
    class GLFrameBuffer;
    class GLShader;

    struct RenderingResource : public IResource
    {
        RenderingResource(IWindow* window);
        ~RenderingResource();

        void Create(IWindow* window);
        void Cleanup(bool closingExe = false);

        std::unique_ptr<Device> device;
        std::unique_ptr<SwapChain> swapChain;
        std::unique_ptr<Synchronizer> syncObjects;

        std::unique_ptr<ModelLibrary> modelLibrary;
        std::unique_ptr<TextureLibrary> textureLibrary;
        std::unique_ptr<AnimationLibrary> animationLibrary;
        std::unique_ptr<RenderGraph> renderGraph;

        // Uniforms ----------------
        std::unique_ptr<Uniform> cameraUniform;
        std::unique_ptr<Uniform> rtUniform;
        std::unique_ptr<Uniform> deferredLightingUniform;
        std::unique_ptr<Uniform> tonemapUniform;
        std::unique_ptr<Uniform> shadowMomentsUniform;
        std::unique_ptr<Uniform> shadowBlurHUniform;
        std::unique_ptr<Uniform> shadowBlurVUniform;
        std::unique_ptr<Uniform> alchemyAOUniform;
        // -------------------------

        std::vector<VkCommandBuffer> commandBuffers;
        uint32_t currentImageIndex = 0;
        uint32_t currentFrameIndex = 0;

        // Scene textures ----------------
        VkImage sceneImage{ VK_NULL_HANDLE };
        VkImageView sceneImageView{ VK_NULL_HANDLE };

        VkImage mDepthImage{ VK_NULL_HANDLE };
        VkImageView mDepthImageView{ VK_NULL_HANDLE };

        VkDescriptorSet sceneTextureDescriptorSet{ VK_NULL_HANDLE };
        // --------------------------------

        // G-Buffer textures ----------------
        VkImage gAlbedoImage{ VK_NULL_HANDLE };
        VkImage gNormalImage{ VK_NULL_HANDLE };
        VkImage gPBRImage{ VK_NULL_HANDLE };
        VkImage gEmissiveImage{ VK_NULL_HANDLE };
        VkImageView gAlbedoImageView{ VK_NULL_HANDLE };
        VkImageView gNormalImageView{ VK_NULL_HANDLE };
        VkImageView gPBRImageView{ VK_NULL_HANDLE };
        VkImageView gEmissiveImageView{ VK_NULL_HANDLE };
        // --------------------------------

        // Pipelines
        std::unique_ptr<Pipeline> pipeline;
        std::unique_ptr<Pipeline> wireframePipeline;
        std::unique_ptr<Pipeline> gBufferPipeline;
        std::unique_ptr<Pipeline> gBufferWireframePipeline;
        std::unique_ptr<Pipeline> deferredLightingPipeline;
        std::unique_ptr<Pipeline> lightVolumePipeline;
        std::unique_ptr<Pipeline> tonemapPipeline;
        std::unique_ptr<ComputePipeline> raytracingPipeline;
        std::unique_ptr<Pipeline> shadowMomentsPipeline;
        std::unique_ptr<ComputePipeline> shadowBlurHPipeline;
        std::unique_ptr<ComputePipeline> shadowBlurVPipeline;
        std::unique_ptr<Pipeline> alchemyAOPipeline;
        // -----------

        // OPENGL STUFFS
        std::unique_ptr<GLShader> shader;
        std::unique_ptr<GLFrameBuffer> sceneFrameBuffer;
        // --------------------------------

        // RT
        std::vector<AccelerationStructure> blasAccel;
        AccelerationStructure tlasAccel;
        // --

        // Render Mode
        RenderMode renderMode = RenderMode::Deferred;
        bool renderWireframe = false;
        uint32_t lightVolumeDebugMode = 0;
        bool supportsVulkan = true;
        int raytraceRenderMode = 1;

        MSMBlurPC msmPC;
        
        // Env map
        uint32_t envMapIndex;
        uint32_t irMapIndex;
        float exposure = 1.0f;
        int useIrrDefuse = 0;
        int specTestMode = 0;

        // Frame data
        int frameCount = 0;
        int accumulationCount = 0; // for path tracing accumulation
        glm::mat4 previousViewProj; // for accumulation

        bool SupportsVulkan();

    private:
        friend class Renderer;
        friend class PresentSystem;
        void RecreateSwapChain(IWindow* window);

        void CreateCommandBuffers();
        VkFormat ToLinearFormat(VkFormat format);
    };
}