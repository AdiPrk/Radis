#include <PCH/pch.h>
#include "EditorResource.h"
#include "RenderingResource.h"
#include "Graphics/Vulkan/VulkanWindow.h"

#include "RenderingResource.h"
#include "WindowResource.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/RenderGraph.h"

#include "Assets/Assets.h"
#include "Engine.h"

namespace Dog
{
    EditorResource::EditorResource(Device* device, SwapChain* swapChain, GLFWwindow* glfwWindow, float dpiScale)
    {
		if (!device || !swapChain || !glfwWindow)
			return;

        Create(device, swapChain, glfwWindow, dpiScale);
		
    }

	EditorResource::EditorResource(GLFWwindow* glfwWindow, float dpiScale)
	{
		if (!glfwWindow) 
			return;

        Create(glfwWindow, dpiScale);
	}

	void EditorResource::Create(Device* device, SwapChain* swapChain, GLFWwindow* glfwWindow, float dpiScale)
	{
		isInitialized = true;

		VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000;
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		vkCreateDescriptorPool(device->GetDevice(), &pool_info, VK_NULL_HANDLE, &descriptorPool);

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 0;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = nullptr;  // Use your own sampler

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &samplerLayoutBinding;

		if (vkCreateDescriptorSetLayout(device->GetDevice(), &layoutInfo, nullptr, &samplerSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout!");
		}

		// init imgui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking
		// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable multi-viewport / platform windows

		ImGui_ImplGlfw_InitForVulkan(glfwWindow, false);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = device->GetInstance();
		init_info.PhysicalDevice = device->GetPhysicalDevice();
		init_info.Device = device->GetDevice();
		init_info.QueueFamily = device->GetGraphicsFamily();
		init_info.Queue = device->GetGraphicsQueue();
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = descriptorPool;// device.getImGuiDescriptorPool();
		init_info.UseDynamicRendering = VK_TRUE;
		init_info.RenderPass = VK_NULL_HANDLE;
		init_info.Subpass = 0;

		VkFormat colorFormat = swapChain->GetImageFormat();
		init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
		init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
		init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = swapChain->GetDepthFormat();
		init_info.PipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

		init_info.Allocator = nullptr;
		init_info.MinImageCount = SwapChain::MAX_FRAMES_IN_FLIGHT;
		init_info.ImageCount = static_cast<uint32_t>(swapChain->ImageCount());
		init_info.CheckVkResultFn = nullptr;
		ImGui_ImplVulkan_Init(&init_info);

		ImGui::StyleColorsDark();

		SetupFonts(dpiScale);
	}

	void EditorResource::Create(GLFWwindow* glfwWindow, float dpiScale)
	{
		isInitialized = true;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOpenGL(glfwWindow, false);  // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
		ImGui_ImplOpenGL3_Init();

		ImGui::StyleColorsDark();

		SetupFonts(dpiScale);
	}

	void EditorResource::Cleanup(Device* device)
	{
		if (!isInitialized) return;

		if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
		{
			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();

			if (samplerSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device->GetDevice(), samplerSetLayout, nullptr);
				samplerSetLayout = VK_NULL_HANDLE;
			}
			if (descriptorPool != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorPool(device->GetDevice(), descriptorPool, nullptr);
				descriptorPool = VK_NULL_HANDLE;
			}
		}
		else
		{
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
		}

		isInitialized = false;
    }
	void EditorResource::CreateSceneTextures(RenderingResource* rr)
	{
		rr->RecreateAllSceneTextures();
	}

	void EditorResource::CleanSceneTextures(RenderingResource* rr)
	{
        rr->CleanupSceneTexture();
		rr->CleanupDepthBuffer();
	}

	void EditorResource::SetupFonts(float dpiScale)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImFontConfig config;
		float fontSize = std::roundf(24.f * dpiScale);
		io.Fonts->AddFontFromFileTTF(std::string(Assets::FontsPath + "Inter_24pt-Regular.ttf").c_str(), fontSize, &config);
	}
}