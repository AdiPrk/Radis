/*****************************************************************//**
 * \file   RenderingResource.cpp
 * \brief  The main rendering resource
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>

#include "RenderingResource.h"
#include "TextureResource.h"
#include "Assets/AssetResource.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/Core/Synchronization.h"
#include "Graphics/Vulkan/Pipeline/Pipeline.h"
#include "Graphics/Vulkan/Pipeline/ComputePipeline.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Texture/VKTexture.h"
#include "Graphics/Vulkan/VulkanWindow.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"
#include "Graphics/Vulkan/Uniform/UniformData.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

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
    RenderingResource::RenderingResource(IWindow* window, AssetResource* assetResource)
    {
        Create(window, assetResource);
    }

    RenderingResource::~RenderingResource()
    {
    }

    void RenderingResource::Create(IWindow* window, AssetResource* assetResource)
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

        if (!textureLibrary)
        {
            // IBL::SHCoefficients sh;
            // IBL::generateIrradianceMap(Assets::ImagesPath + "Alexs_Apt_2k.hdr", Assets::ImagesPath + "Alexs_Apt_2k.IRMAP.hdr", &sh);
            // IBL::generateIrradianceMap(Assets::ImagesPath + "Newport_Loft_Ref.hdr", Assets::ImagesPath + "Newport_Loft_Ref.IRMAP.hdr", &sh);
            // IBL::generateIrradianceMap(Assets::ImagesPath + "autumn_field_puresky_4k.hdr", Assets::ImagesPath + "autumn_field_puresky_4k.IRMAP.hdr", &sh);

            textureLibrary = std::make_unique<TextureLibrary>(device.get());
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Placeholder.png");
            // textureLibrary->QueueTextureLoad(Assets::ImagesPath + "ErrorTexture.png"); trying thru asset resource instead
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
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "autumn_field_puresky_4k.hdr");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Alexs_Apt_2k.hdr");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Newport_Loft_Ref.hdr");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "autumn_field_puresky_4k.IRMAP.hdr");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Alexs_Apt_2k.IRMAP.hdr");
            textureLibrary->QueueTextureLoad(Assets::ImagesPath + "Newport_Loft_Ref.IRMAP.hdr");

            textureLibrary->LoadQueuedTextures();

            // TODO: dds
            //textureLibrary->QueueTextureLoad(Assets::ImagesPath + "M_Soul_Rocks2_Inst_8_BaseColor.dds");
        }
        else
        {
            textureLibrary->SetDevice(device.get());
        }

        if (!modelLibrary)
        {
            modelLibrary = std::make_unique<ModelLibrary>(*device, *textureLibrary);
            modelLibrary->SetAssetResource(assetResource);
            modelLibrary->AddModel(Assets::ModelsPath + "cube.obj", true);
            modelLibrary->AddModel(Assets::ModelsPath + "quad.obj", true);
            modelLibrary->AddModel(Assets::ModelsPath + "sphere.obj", true);
            modelLibrary->AddModel(Assets::ModelsPath + "pbrreference.glb", true);
            modelLibrary->AddModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx", true);
            modelLibrary->AddModel(Assets::ModelsPath + "TakanashiKiara/TakanashiKiara.fbx", true);
            modelLibrary->AddModel(Assets::ModelsPath + "jack_samba.glb", true);
            modelLibrary->AddModel(Assets::ModelsPath + "SteampunkRobot.gltf", true);
            modelLibrary->AddModel(Assets::ModelsPath + "DragonAttenuation.glb", true);
            modelLibrary->AddModel(Assets::ModelsPath + "Sponza.gltf", true);
            modelLibrary->AddModel(Assets::ModelsPath + "okayu/okayu.fbx", true);
            // modelLibrary->AddModel(Assets::ModelsPath + "sanmiguellow.glb", true);
            // modelLibrary->AddModel(Assets::ModelsPath + "NewSponza_Curtains.dm", true, false);
            // modelLibrary->AddModel(Assets::ModelsPath + "NewSponza_Main.dm", true);

            modelLibrary->InitializeUnifiedMesh();
        }

        if (!animationLibrary)
        {
            animationLibrary = std::make_unique<AnimationLibrary>();
            Model* travisModel = modelLibrary->GetModel(Assets::ModelsPath + "TravisLocomotion/TravisLocomotion.fbx");
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/idle.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/idle.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/jump.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/left strafe walking.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/left strafe.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/left turn 90.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/right strafe walking.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/right strafe.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/right turn 90.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/standard run.fbx", travisModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TravisLocomotion/walking.fbx", travisModel);

            Model* okayuModel = modelLibrary->GetModel(Assets::ModelsPath + "okayu/okayu.fbx");
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/idle.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/idle.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/jump.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/left strafe walking.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/left strafe.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/left turn 90.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/right strafe walking.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/right strafe.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/right turn 90.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/standard run.fbx", okayuModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "okayu/walking.fbx", okayuModel);

            Model* kiaraModel = modelLibrary->GetModel(Assets::ModelsPath + "TakanashiKiara/TakanashiKiara.fbx");
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/idle.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/jump.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/left strafe walk.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/left strafe.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/left turn (2).fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/left turn.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/right strafe walk.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/right strafe.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/right turn (2).fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/right turn.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/running.fbx", kiaraModel);
            animationLibrary->AddAnimation(Assets::ModelsPath + "TakanashiKiara/walking.fbx", kiaraModel);

            animationLibrary->AddAnimation(Assets::ModelsPath + "jack_samba.glb", modelLibrary->GetModel(Assets::ModelsPath + "jack_samba.glb"));
        }

        // Recreation if needed
        textureLibrary->CreateTextureSampler();
        textureLibrary->CreateDescriptors();

        if (swapChain)
        {
            VkExtent2D extent = swapChain->GetSwapChainExtent();

            // HDR format for scene color (before tonemapping)
            VkFormat hdrFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            VkFormat momentsFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

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

            // AO
            textureLibrary->CreateTexture(
                "RawAO",
                extent.width, extent.height,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            // AO Blur Textures
            textureLibrary->CreateTexture(
                "AOBlurTmp",
                extent.width, extent.height,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            textureLibrary->CreateTexture(
                "BlurredAO",
                extent.width, extent.height,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
            {
                textureLibrary->CreateTexture(
                    "RTAccum_" + std::to_string(i),
                    extent.width, extent.height,
                    VK_FORMAT_R32G32B32A32_SFLOAT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_LAYOUT_GENERAL
                );

                textureLibrary->CreateTexture(
                    "RTHeatmapImage_" + std::to_string(i),
                    extent.width, extent.height,
                    VK_FORMAT_R32G32B32A32_SFLOAT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_LAYOUT_GENERAL
                );
            }
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
            
            std::vector<Uniform*> unis{ cameraUniform.get() };
            std::vector<Uniform*> rtunis{ cameraUniform.get(), rtUniform.get() };
            std::vector<Uniform*> deferredLightingUnis{ deferredLightingUniform.get() };
            std::vector<Uniform*> tonemapUnis{ tonemapUniform.get() };
            
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

            pipeline = std::make_unique<Pipeline>(*device, ldrFormat, swapDepthFormat, unis, false, "forward.vert", "forward.frag");
            wireframePipeline = std::make_unique<Pipeline>(*device, ldrFormat, swapDepthFormat, unis, true, "forward.vert", "forward.frag");
            
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

        alchemyAOUniform = std::make_unique<Uniform>(*device, *this, alchemyAOUniformSettings);
        std::vector<Uniform*> aoUnis{ cameraUniform.get(), alchemyAOUniform.get() };

        PipelineOptions aoOpts;
        aoOpts.pushConstantSize = 20; // radius, numSamples, scale, contrast, debug mode
        aoOpts.pushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT;

        alchemyAOPipeline = std::make_unique<Pipeline>(*device, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_UNDEFINED, aoUnis, false, "fullscreen.vert", "AO.frag", aoOpts, false);

        aoBlurHUniform = std::make_unique<Uniform>(*device, *this, aoBlurHUniformSettings);
        aoBlurVUniform = std::make_unique<Uniform>(*device, *this, aoBlurVUniformSettings);

        std::vector<Uniform*> blurHUnis{ cameraUniform.get(), aoBlurHUniform.get() };
        std::vector<Uniform*> blurVUnis{ cameraUniform.get(), aoBlurVUniform.get() };

        PipelineOptions blurOpts;
        blurOpts.pushConstantSize = 20; // 2 floats + 1 int + 2 floats = 20 bytes
        blurOpts.pushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT;

        aoBlurHPipeline = std::make_unique<Pipeline>(*device, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_UNDEFINED, blurHUnis, false, "fullscreen.vert", "bilateralBlur.frag", blurOpts, false);
        aoBlurVPipeline = std::make_unique<Pipeline>(*device, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_UNDEFINED, blurVUnis, false, "fullscreen.vert", "bilateralBlur.frag", blurOpts, false);
    }

    void RenderingResource::Shutdown()
    {
        if (!device || !device->GetDevice())
            return;

        vkDeviceWaitIdle(*device);

        for (auto& blas : blasAccel)
        {
            Allocator::DestroyAcceleration(blas);
        }
        blasAccel.clear();

        if (tlasAccel.accel != VK_NULL_HANDLE)
        {
            Allocator::DestroyAcceleration(tlasAccel);
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
