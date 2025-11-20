#include <PCH/pch.h>

#include "RenderingResource.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/Core/Synchronization.h"
#include "Graphics/Vulkan/Pipeline/Pipeline.h"
#include "Graphics/Vulkan/Pipeline/RaytracingPipeline.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Texture/VKTexture.h"
#include "Graphics/Vulkan/VulkanWindow.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"
#include "Graphics/Vulkan/Uniform/UniformData.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"

#include "Graphics/OpenGL/GLFrameBuffer.h"

#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/Common/Animation/AnimationLibrary.h"
#include "Graphics/Common/Animation/Animator.h"
#include "Graphics/Common/Model.h"

#include "Assets/Assets.h"
#include "Engine.h"

namespace Dog
{
    RenderingResource::RenderingResource(IWindow* window)
    {
        Create(window);
    }

    RenderingResource::~RenderingResource()
    {
        Cleanup(true);
    }

    void RenderingResource::Create(IWindow* window)
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            device = std::make_unique<Device>(*dynamic_cast<VulkanWindow*>(window));

            if (!device->SupportsVulkan())
            {
                supportsVulkan = false;
                device.reset();
                Engine::ForceVulkanUnsupportedSwap();
                return;
            }

            RecreateSwapChain(window);

            VkFormat srgbFormat = swapChain->GetImageFormat();
            VkFormat linearFormat = ToLinearFormat(srgbFormat);
            device->SetFormats(linearFormat, srgbFormat);

            syncObjects = std::make_unique<Synchronizer>(device->GetDevice(), swapChain->ImageCount());
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            GLShader::SetupUBO();
            shader = std::make_unique<GLShader>();
            shader->load("Assets/shaders/verysimple.vert", "Assets/shaders/verysimple.frag");

            FrameBufferSpecification fbSpec;
            fbSpec.width = 1280;
            fbSpec.height = 720;
            fbSpec.samples = 1;
            fbSpec.attachments = { FBAttachment::RGBA8_SRGB, FBAttachment::Depth24Stencil8 };
            sceneFrameBuffer = std::make_unique<GLFrameBuffer>(fbSpec);
        }

        bool recreateTextures = textureLibrary != nullptr;
        if (!textureLibrary)
        {
            textureLibrary = std::make_unique<TextureLibrary>(device.get());
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "circle.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "dog.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "circleOutline2.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "dogmodel.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "error.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "ErrorTexture.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "glslIcon.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "playButton.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "square.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "stopButton.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "texture.jpg");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "folderIcon.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "unknownFileIcon.png");
        }
        else
        {
            textureLibrary->SetDevice(device.get());
        }

        if (!modelLibrary)
        {
            modelLibrary = std::make_unique<ModelLibrary>(*device, *textureLibrary);

            modelLibrary->AddModel(Assets::ModelsPath + "cube.obj");
            modelLibrary->AddModel(Assets::ModelsPath + "quad.obj");
            modelLibrary->AddModel(Assets::ModelsPath + "sphere.glb");
            modelLibrary->AddModel(Assets::ModelsPath + "sphere.obj");
            modelLibrary->AddModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx");
            modelLibrary->AddModel(Assets::ModelsPath + "jack_samba.glb");
            modelLibrary->AddModel(Assets::ModelsPath + "SteampunkRobot.gltf");
            modelLibrary->AddModel(Assets::ModelsPath + "DragonAttenuation.glb");
            modelLibrary->AddModel(Assets::ModelsPath + "Sponza.gltf");

            // modelLibrary->AddModel("Assets/Models/okayu.pmx");
            // modelLibrary->AddModel("Assets/Models/AlisaMikhailovna.fbx");
        }

        if (!animationLibrary)
        {
            animationLibrary = std::make_unique<AnimationLibrary>();
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/idle.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/idle.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/jump.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/left strafe walking.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/left strafe.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/left turn 90.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/right strafe walking.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/right strafe.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/right turn 90.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/standard run.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/walking.fbx", modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx"));
            animationLibrary->AddAnimation(Assets::ModelsPath + "jack_samba.glb", modelLibrary->GetModel(Assets::ModelsPath + "jack_samba.glb"));
        }

        // Recreation if needed
        textureLibrary->CreateTextureSampler();
        textureLibrary->CreateDescriptors();
        if (recreateTextures)
        {
            textureLibrary->RecreateAllBuffers(device.get());
        }
        if (swapChain)
        {
            VkExtent2D extent = swapChain->GetSwapChainExtent();

            // Create scene and depth textures
            textureLibrary->CreateTexture(
                "SceneTexture",                 // name
                extent.width,                   // width
                extent.height,                  // height
                device->GetLinearFormat(),      // format
                VK_IMAGE_TILING_OPTIMAL,        // tiling
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // usage
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // final layout
            );
            textureLibrary->CreateTexture(
                "SceneDepth",                 // name
                extent.width,                 // width
                extent.height,                // height
                swapChain->FindDepthFormat(), // format
                VK_IMAGE_TILING_OPTIMAL,      // tiling
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // usage
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL // final layout
            );
        }

        modelLibrary->QueueTextures();
        
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            CreateCommandBuffers();
            renderGraph = std::make_unique<RenderGraph>();

            cameraUniform = std::make_unique<Uniform>(*device, *this, cameraUniformSettings);
            rtUniform = std::make_unique<Uniform>(*device, *this, rayTracingUniformSettings);

            std::vector<Uniform*> unis{
                cameraUniform.get(),
            };
            std::vector<Uniform*> rtunis{
                cameraUniform.get(),
                rtUniform.get()
            };

            pipeline = std::make_unique<Pipeline>(
                *device,
                swapChain->GetImageFormat(), swapChain->FindDepthFormat(),
                unis,
                false,
                "verysimple.vert", "verysimple.frag"
            );

            wireframePipeline = std::make_unique<Pipeline>(
                *device,
                swapChain->GetImageFormat(), swapChain->FindDepthFormat(),
                unis,
                true,
                "verysimple.vert", "verysimple.frag"
            );

            raytracingPipeline = std::make_unique<RaytracingPipeline>(
                *device,
                rtunis
            );
        }
    }

    void RenderingResource::Cleanup(bool closingExe)
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            if (!device)
            {
                return;
            }

            if (device->GetDevice()) 
            {
                vkDeviceWaitIdle(*device);
            }

            vkFreeCommandBuffers(
                device->GetDevice(),
                device->GetCommandPool(),
                static_cast<uint32_t>(commandBuffers.size()),
                commandBuffers.data()
            );

            if (!closingExe)
            {
                if (modelLibrary) modelLibrary->ClearAllBuffers(device.get());
                if (textureLibrary) textureLibrary->ClearAllBuffers(device.get());
            }
            else
            {
                if (modelLibrary) modelLibrary->ClearAllBuffers(device.get());
                if (textureLibrary) textureLibrary->ClearAllBuffers(device.get());
                modelLibrary.reset();
                textureLibrary.reset();
                animationLibrary.reset();
            }
            renderGraph.reset();
            cameraUniform.reset();
            rtUniform.reset();
            pipeline.reset();
            wireframePipeline.reset();
            raytracingPipeline.reset();
            syncObjects.reset();

            for (auto& blas : blasAccel)
            {
                Allocator::DestroyAcceleration(blas);
            }
            blasAccel.clear();
            if (tlasAccel.accel != VK_NULL_HANDLE) {
                Allocator::DestroyAcceleration(tlasAccel);
            }

            swapChain.reset();
            device.reset();
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            //modelLibrary.reset();
            //textureLibrary.reset();
            if (modelLibrary) modelLibrary->ClearAllBuffers(device.get());
            if (textureLibrary) textureLibrary->ClearAllBuffers(device.get());
            sceneFrameBuffer.reset();
            shader.reset();
            GLShader::CleanupUBO();
        }
    }

    bool RenderingResource::SupportsVulkan()
    {
        return supportsVulkan;
    }

    void RenderingResource::RecreateSwapChain(IWindow* window)
    {
        auto extent = window->GetExtent();
        while (extent.x == 0 || extent.y == 0) {
            extent = window->GetExtent();
            window->WaitEvents();
        }

        vkDeviceWaitIdle(*device);

        if (swapChain == nullptr) {
            swapChain = std::make_unique<SwapChain>(*device, extent);
        }
        else {
            std::shared_ptr<SwapChain> oldSwapChain = std::move(swapChain);
            swapChain = std::make_unique<SwapChain>(*device, extent, oldSwapChain);

            if (!oldSwapChain->CompareSwapFormats(*swapChain.get())) {
                DOG_ERROR("Swap chain image(or depth) format has changed!");
            }
        }
    }
    void RenderingResource::CreateCommandBuffers()
    {
        //Resize command buffer to match number of possible frames in flight
        commandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        //Create struct for holding allocation info for this command buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = device->GetCommandPool();
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        //Allocate the command buffers
        if (vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            DOG_CRITICAL("Failed to allocate command buffers");
        }
    }

    VkFormat RenderingResource::ToLinearFormat(VkFormat format)
    {
        if (format == VK_FORMAT_R8G8B8A8_SRGB) {
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
        if (format == VK_FORMAT_B8G8R8A8_SRGB) {
            return VK_FORMAT_B8G8R8A8_UNORM;
        }

        DOG_CRITICAL("Unsupported format for sRGB conversion");
        return format;
    }

}
