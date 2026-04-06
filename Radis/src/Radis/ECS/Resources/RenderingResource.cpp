/*****************************************************************//**
 * \file   RenderingResource.cpp
 * \brief  The main rendering resource
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>

#include "RenderingResource.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/Core/Synchronization.h"
#include "Graphics/Vulkan/Pipeline/Pipeline.h"
#include "Graphics/Vulkan/Pipeline/ComputePipeline.h"
#include "Graphics/Vulkan/Pipeline/RaytracingPipeline.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Texture/VKTexture.h"
#include "Graphics/Vulkan/VulkanWindow.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"
#include "Graphics/Vulkan/Uniform/UniformData.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

#include "Graphics/OpenGL/GLFrameBuffer.h"

#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/Common/Animation/AnimationLibrary.h"
#include "Graphics/Common/Animation/Animator.h"
#include "Graphics/Common/Model.h"
#include "Graphics/Common/IBL/IrradianceMap.h"

#include "Assets/Assets.h"
#include "Engine.h"

#include "Assets/Serialization/ModelSerializer.h"

namespace Radis
{
    RenderingResource::RenderingResource(IWindow* window)
        : envMapIndex{ TextureLibrary::INVALID_TEXTURE_INDEX }
        , irMapIndex{ TextureLibrary::INVALID_TEXTURE_INDEX }
    {
        Create(window);
    }

    RenderingResource::~RenderingResource()
    {
        Cleanup(true);
    }

    void RenderingResource::Create(IWindow* window)
    {
        msmPC.radius = 4;

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
            shader->load("Assets/Shaders/forward.vert", "Assets/shaders/forward.frag");

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
            // IBL::SHCoefficients sh;
            // IBL::generateIrradianceMap(Assets::ImagesPath + "Alexs_Apt_2k.hdr", Assets::ImagesPath + "Alexs_Apt_2k.IRMAP.hdr", &sh);
            // IBL::generateIrradianceMap(Assets::ImagesPath + "Newport_Loft_Ref.hdr", Assets::ImagesPath + "Newport_Loft_Ref.IRMAP.hdr", &sh);
            // IBL::generateIrradianceMap(Assets::ImagesPath + "autumn_field_puresky_4k.hdr", Assets::ImagesPath + "autumn_field_puresky_4k.IRMAP.hdr", &sh);

            textureLibrary = std::make_unique<TextureLibrary>(device.get());
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "ErrorTexture.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "circle.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "dog.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "circleOutline2.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "dogmodel.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "error.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "square.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "glslIcon.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "playButton.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "stopButton.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "texture.jpg");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "folderIcon.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "unknownFileIcon.png");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "shikaout.ktx2");
            envMapIndex = textureLibrary->QueueTextureLoad(Assets::ImagesPath + "autumn_field_puresky_4k.hdr");
            envMapIndex = textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Alexs_Apt_2k.hdr");
            envMapIndex = textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Newport_Loft_Ref.hdr");
            irMapIndex = textureLibrary->QueueTextureLoad(Assets::ImagesPath + "autumn_field_puresky_4k.IRMAP.hdr");
            irMapIndex = textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Alexs_Apt_2k.IRMAP.hdr");
            irMapIndex = textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Newport_Loft_Ref.IRMAP.hdr");


            textureLibrary->LoadQueuedTextures();
            //textureLibrary->QueueTextureLoad(Assets::ImagesPath + "M_Soul_Rocks2_Inst_8_BaseColor.dds");
        }
        else
        {
            textureLibrary->SetDevice(device.get());
        }

        if (!modelLibrary)
        {
            modelLibrary = std::make_unique<ModelLibrary>(*device, *textureLibrary);

            modelLibrary->AddModel(Assets::ModelsPath + "cube.obj", true);
            modelLibrary->AddModel(Assets::ModelsPath + "quad.obj", true);
            modelLibrary->AddModel(Assets::ModelsPath + "sphere.glb", true);
            modelLibrary->AddModel(Assets::ModelsPath + "pbrreference.glb", true);
            modelLibrary->AddModel(Assets::ModelsPath + "trotting_cat.glb");
            modelLibrary->AddModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx", true);
            modelLibrary->AddModel(Assets::ModelsPath + "jack_samba.glb", true);
            modelLibrary->AddModel(Assets::ModelsPath + "SteampunkRobot.gltf", true);
            modelLibrary->AddModel(Assets::ModelsPath + "DragonAttenuation.glb", true);
            modelLibrary->AddModel(Assets::ModelsPath + "Sponza.gltf", true);
            // modelLibrary->AddModel(Assets::ModelsPath + "NewSponza_Curtains.gltf", true, false, false);
            // modelLibrary->AddModel(Assets::ModelsPath + "NewSponza_Main.gltf", true, false);

            // Model* sponzaModel = modelLibrary->GetModel(sponzaInd);
            //VFS::ModelSerializer::save(*sponzaModel, "Assets/Models/dm/Sponza.dm", 0xDEADBEEF);
            // load it to test
            //Model newModelTest;
            //VFS::ModelSerializer::load(newModelTest, "Assets/Models/dm/Sponza.dm");
            //printf("done"); 

            // modelLibrary->AddModel("Assets/Models/okayu.pmx");
            // modelLibrary->AddModel("Assets/Models/AlisaMikhailovna.fbx");
        }

        if (!animationLibrary)
        {
            animationLibrary = std::make_unique<AnimationLibrary>();
            animationLibrary->AddAnimation(Assets::ModelsPath + "trotting_cat.glb", modelLibrary->GetModel(Assets::ModelsPath + "trotting_cat.glb"));
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

            // HDR format for scene color (before tonemapping)
            VkFormat hdrFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            VkFormat momentsFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

            // Choose shadow resolution
            uint32_t shadowW = 2048;
            uint32_t shadowH = 2048;

            // Raw moments written by raster pass
            textureLibrary->CreateTexture(
                "ShadowMomentsRaw",
                shadowW, shadowH,
                momentsFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            // Temp (H blur output)
            textureLibrary->CreateTexture(
                "ShadowMomentsTmp",
                shadowW, shadowH,
                momentsFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            // Final blurred moments sampled by lighting
            textureLibrary->CreateTexture(
                "ShadowMoments",
                shadowW, shadowH,
                momentsFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            // Optional depth for shadow pass (recommended)
            textureLibrary->CreateTexture(
                "ShadowDepth",
                shadowW, shadowH,
                swapChain->FindDepthFormat(),
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            );

            // Create HDR scene texture
            textureLibrary->CreateTexture(
                "SceneTexture",
                extent.width, extent.height,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            textureLibrary->CreateTexture(
                "SceneDepth",
                extent.width, extent.height,
                swapChain->FindDepthFormat(),
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            );

            // G-Buffer textures
            textureLibrary->CreateTexture(
                "gAlbedo",
                extent.width, extent.height,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            textureLibrary->CreateTexture(
                "gNormal",
                extent.width, extent.height,
                VK_FORMAT_R16G16_SFLOAT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            textureLibrary->CreateTexture(
                "gPBR",
                extent.width, extent.height,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            textureLibrary->CreateTexture(
                "gEmissive",
                extent.width, extent.height,
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            textureLibrary->CreateTexture(
                "SceneHDR",
                extent.width, extent.height,
                hdrFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
        }

        modelLibrary->QueueTextures();
        
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            CreateCommandBuffers();
            renderGraph = std::make_unique<RenderGraph>();

            cameraUniform = std::make_unique<Uniform>(*device, *this, cameraUniformSettings);
            rtUniform = std::make_unique<Uniform>(*device, *this, rayTracingUniformSettings);
            deferredLightingUniform = std::make_unique<Uniform>(*device, *this, deferredLightingUniformSettings);
            tonemapUniform = std::make_unique<Uniform>(*device, *this, tonemapUniformSettings);
            shadowMomentsUniform = std::make_unique<Uniform>(*device, *this, shadowMomentsUniformSettings);
            shadowBlurHUniform = std::make_unique<Uniform>(*device, *this, shadowBlurHUniformSettings);
            shadowBlurVUniform = std::make_unique<Uniform>(*device, *this, shadowBlurVUniformSettings);

            std::vector<Uniform*> unis{ cameraUniform.get() };
            std::vector<Uniform*> rtunis{ cameraUniform.get(), rtUniform.get() };
            std::vector<Uniform*> deferredLightingUnis{ deferredLightingUniform.get() };
            std::vector<Uniform*> tonemapUnis{ tonemapUniform.get() };
            std::vector<Uniform*> shadowUnis{ shadowMomentsUniform.get() };
            std::vector<Uniform*> blurHUnis{ shadowBlurHUniform.get() };
            std::vector<Uniform*> blurVUnis{ shadowBlurVUniform.get() };
            
            VkFormat swapImageFormat = swapChain->GetImageFormat();
            VkFormat swapDepthFormat = swapChain->FindDepthFormat();
            VkFormat momentsFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            VkFormat hdrFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            VkFormat ldrFormat = VK_FORMAT_R8G8B8A8_UNORM;
            VkFormat albedoFormat = VK_FORMAT_R8G8B8A8_SRGB;
            VkFormat normalFormat = VK_FORMAT_R16G16_SFLOAT;
            VkFormat pbrFormat = VK_FORMAT_R8G8B8A8_UNORM;
            VkFormat emissiveFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            std::vector<VkFormat> gBufferFormats = { albedoFormat, normalFormat, pbrFormat, emissiveFormat };

            pipeline = std::make_unique<Pipeline>(*device, hdrFormat, swapDepthFormat, unis, false, "forward.vert", "forward.frag");
            wireframePipeline = std::make_unique<Pipeline>(*device, hdrFormat, swapDepthFormat, unis, true, "forward.vert", "forward.frag");
            
            shadowMomentsPipeline = std::make_unique<Pipeline>(*device, momentsFormat, swapDepthFormat, shadowUnis, false, "shadowMoments.vert", "shadowMoments.frag");

            PushConstantInfo computePC;
            computePC.size = sizeof(MSMBlurPC);
            computePC.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            shadowBlurHPipeline = std::make_unique<ComputePipeline>(*device, blurHUnis, "msmBlurH.comp", computePC);
            shadowBlurVPipeline = std::make_unique<ComputePipeline>(*device, blurVUnis, "msmBlurV.comp", computePC);
            
            gBufferPipeline = std::make_unique<Pipeline>(*device, gBufferFormats, swapDepthFormat, unis, false, "deferred.vert", "deferred.frag");
            gBufferWireframePipeline = std::make_unique<Pipeline>(*device, gBufferFormats, swapDepthFormat, unis, true, "deferred.vert", "deferred.frag");
            
            {
                PipelineOptions defLightOpts;
                defLightOpts.pushConstantSize = sizeof(int) * 2;
                defLightOpts.pushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT;

                deferredLightingPipeline = std::make_unique<Pipeline>(*device, hdrFormat, VK_FORMAT_UNDEFINED, deferredLightingUnis, false, "fullscreen.vert", "deferredLight.frag", defLightOpts, false);
            }

            // Light volume pipeline: additive blend, no depth test, front-face culling, push constants
            {
                PipelineOptions lightVolOpts;
                lightVolOpts.additiveBlend = true;
                lightVolOpts.depthTestDisable = true;
                lightVolOpts.depthWriteDisable = true;
                lightVolOpts.cullFrontFace = true;
                lightVolOpts.pushConstantSize = sizeof(LightVolumePushConstants);
                lightVolOpts.pushConstantStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                lightVolumePipeline = std::make_unique<Pipeline>(*device, hdrFormat, VK_FORMAT_UNDEFINED, deferredLightingUnis, false, "lightVolume.vert", "lightVolume.frag", lightVolOpts);
            }

            // Tone mapping: reads SceneHDR, outputs final color
            {
                PipelineOptions tonemapOpts;
                tonemapOpts.pushConstantSize = sizeof(float);
                tonemapOpts.pushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT;

                tonemapPipeline = std::make_unique<Pipeline>(*device, ldrFormat, VK_FORMAT_UNDEFINED, tonemapUnis, false, "fullscreen.vert", "tonemap.frag", tonemapOpts, false);
            }

            // raytracingPipeline = std::make_unique<RaytracingPipeline>(*device, rtunis);

            PushConstantInfo rtPC{};
            rtPC.size = sizeof(int);
            rtPC.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            raytracingPipeline = std::make_unique<ComputePipeline>(*device, rtunis, "rayquery.comp", rtPC);
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
            shadowMomentsUniform.reset();
            shadowBlurHUniform.reset();
            shadowBlurVUniform.reset();
            deferredLightingUniform.reset();
            tonemapUniform.reset();
            pipeline.reset();
            wireframePipeline.reset();
            shadowMomentsPipeline.reset();
            shadowBlurHPipeline.reset();
            shadowBlurVPipeline.reset();
            gBufferPipeline.reset();
            gBufferWireframePipeline.reset();
            deferredLightingPipeline.reset();
            lightVolumePipeline.reset();
            raytracingPipeline.reset();
            tonemapPipeline.reset();
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
                RADIS_ERROR("Swap chain image(or depth) format has changed!");
            }
        }
    }

    void RenderingResource::CreateCommandBuffers()
    {
        commandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = device->GetCommandPool();
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            RADIS_CRITICAL("Failed to allocate command buffers");
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

        RADIS_CRITICAL("Unsupported format for sRGB conversion");
        return format;
    }
}
