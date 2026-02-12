/*****************************************************************//**
 * \file   Pipeline.h
 * \brief  Definition of the Pipeline class for Vulkan graphics pipelines.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "../Core/Device.h"

namespace Radis
{
	class Uniform;
	class Device;

	// Optional overrides for pipeline creation
	struct PipelineOptions
	{
		bool additiveBlend = false;
		bool depthTestDisable = false;
		bool depthWriteDisable = false;
		bool cullFrontFace = false;
		uint32_t pushConstantSize = 0;
		VkShaderStageFlags pushConstantStages = 0;
	};

	//Holds also configuration info for a pipeline
	struct PipelineConfigInfo
	{
		//Delete copy operations
		PipelineConfigInfo(const PipelineConfigInfo&) = delete;
		PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

		/*********************************************************************
		 * brief:  Create config info object
		 *********************************************************************/
		PipelineConfigInfo()
			: viewportCreateInfo(),
			inputAssemblyCreateInfo(),
			rasterizationCreateInfo(),
			multisampleCreateInfo(),
			colorBlendAttachment(),
			colorBlendCreateInfo(),
			depthStencilCreateInfo(),
			dynamicStateCreateInfo(),
			dynamicStateEnables(),
            pipeLineLayout(nullptr),
            renderPass(nullptr),
            depthFormat(VK_FORMAT_UNDEFINED),
            subpass(0)
		{
		};

		VkPipelineViewportStateCreateInfo viewportCreateInfo;
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo;
		VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo;
		VkPipelineMultisampleStateCreateInfo multisampleCreateInfo;
		VkPipelineColorBlendAttachmentState colorBlendAttachment;
		VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo;
		VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo;
		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
		std::vector<VkDynamicState> dynamicStateEnables;
		VkPipelineLayout pipeLineLayout = nullptr;
		VkRenderPass renderPass = nullptr;
		std::vector<VkFormat> colorFormats;
		VkFormat depthFormat;
		uint32_t subpass = 0;
	};

	class Pipeline
	{
	public:
		// Shader Directories
		inline static const std::string ShaderDir = "Assets/shaders/";
		inline static const std::string SpvDir = "Assets/shaders/spv/";

		Pipeline(Device& device, const std::vector<VkFormat>& colorFormats, VkFormat depthFormat, const std::vector<Uniform*>& uniforms, bool wireframe, const std::string& vertFile, const std::string& fragFile, bool useVertexInput = true);
		Pipeline(Device& device, VkFormat colorFormat, VkFormat depthFormat, const std::vector<Uniform*>& uniforms, bool wireframe, const std::string& vertFile, const std::string& fragFile, bool useVertexInput = true);
		Pipeline(Device& device, VkFormat colorFormat, VkFormat depthFormat, const std::vector<Uniform*>& uniforms, bool wireframe, const std::string& vertFile, const std::string& fragFile, const std::string& tescFile, const std::string& teseFile, bool useVertexInput = true);

		// Constructor with PipelineOptions for custom blend/depth/cull/push-constant settings
		Pipeline(Device& device, VkFormat colorFormat, VkFormat depthFormat, const std::vector<Uniform*>& uniforms, const std::string& vertFile, const std::string& fragFile, const PipelineOptions& options, bool useVertexInput = true);

		void DestroyPipeline();
		void Recreate();

		~Pipeline();

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		void Bind(VkCommandBuffer commandBuffer);

		VkPipelineLayout& GetLayout() { return mPipelineLayout; };
		const std::vector<VkFormat>& GetColorFormats() const { return mColorFormats; }
		uint32_t GetColorAttachmentCount() const { return static_cast<uint32_t>(mColorFormats.size()); }

	private:
		void CreatePipelineLayout(const std::vector<Uniform*>& uniforms);
		void CreatePipeline();
		void DefaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
		void CreateGraphicsPipeline(const PipelineConfigInfo& configInfo);
		
		Device& device;
		VkPipeline mGraphicsPipeline;		         
		VkShaderModule mVertShaderModule;            
		VkShaderModule mFragShaderModule;            
		VkShaderModule mTessCtrlShaderModule = NULL; 
		VkShaderModule mTessEvalShaderModule = NULL; 
		VkPipelineLayout mPipelineLayout;  
		bool isWireframe;

		// Shader paths
		std::string mVertPath;
		std::string mFragPath;
		std::string mTescPath;
		std::string mTesePath;
		std::string mSpvVertPath;
		std::string mSpvFragPath;
		std::string mSpvTescPath;
		std::string mSpvTesePath;

		// Pipeline info
        std::vector<VkFormat> mColorFormats;
        VkFormat mDepthFormat;
        bool mIsWireframe;
        bool mUseVertexInput = true;
        const std::vector<Uniform*>& mUniforms;

		// Optional overrides
		PipelineOptions mOptions{};
	};
}