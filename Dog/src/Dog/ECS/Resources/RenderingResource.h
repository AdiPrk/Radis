#pragma once

#include "IResource.h"

#include "Graphics/OpenGL/GLShader.h"

namespace Dog
{
    class IWindow;
    class Device;
    class SwapChain;
    class Synchronizer;
    class Pipeline;
    class Renderer;
    class RenderGraph;
    class Allocator;
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
        void Cleanup();

        std::unique_ptr<Device> device;
        std::unique_ptr<SwapChain> swapChain;
        std::unique_ptr<Synchronizer> syncObjects;
        std::unique_ptr<Allocator> allocator;

        std::unique_ptr<ModelLibrary> modelLibrary;
        std::unique_ptr<TextureLibrary> textureLibrary;
        std::unique_ptr<AnimationLibrary> animationLibrary;
        std::unique_ptr<RenderGraph> renderGraph;
        
        // Uniforms ----------------
        std::unique_ptr<Uniform> cameraUniform;
        //std::unique_ptr<Uniform> instanceUniform;
        // -------------------------

        std::vector<VkCommandBuffer> commandBuffers;
        uint32_t currentImageIndex = 0;
        uint32_t currentFrameIndex = 0;

        // Scene textures ----------------
        VkImage sceneImage{ VK_NULL_HANDLE };
        VmaAllocation sceneImageAllocation{ VK_NULL_HANDLE };
        VkImageView sceneImageView{ VK_NULL_HANDLE };
        VkSampler sceneSampler{ VK_NULL_HANDLE };

        VkImage mDepthImage{ VK_NULL_HANDLE };
        VmaAllocation mDepthImageAllocation{ VK_NULL_HANDLE };
        VkImageView mDepthImageView{ VK_NULL_HANDLE };

        VkDescriptorSet sceneTextureDescriptorSet{ VK_NULL_HANDLE };
        // --------------------------------

        // Pipelines
        std::unique_ptr<Pipeline> pipeline;
        std::unique_ptr<Pipeline> wireframePipeline;
        // -----------

        // OPENGL STUFFS
        std::unique_ptr<GLShader> shader;
        std::unique_ptr<GLFrameBuffer> sceneFrameBuffer;
        // --------------------------------

        bool renderWireframe = false;

        // Texture update
        void UpdateTextureUniform();

    private:
        friend class Renderer;
        friend class PresentSystem;
        void RecreateSwapChain(IWindow* window);

        void CreateCommandBuffers();

        // Scene textures ----------------
        friend struct EditorResource;
        void CreateSceneTexture();
        void CleanupSceneTexture();
        void RecreateSceneTexture();
        void CreateDepthBuffer();
        void CleanupDepthBuffer();
        void RecreateDepthBuffer();
        void RecreateAllSceneTextures();
        // --------------------------------
        
        VkFormat ToLinearFormat(VkFormat format);

    };
}
