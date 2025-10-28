#include <PCH/pch.h>
#include "Pipeline.h"
#include "Shader.h"

#include "../Core/Device.h"
#include "../Model/Mesh.h"

#include "../Uniform/Uniform.h"
#include "../Uniform/Descriptors.h"

namespace Dog
{
	Pipeline::Pipeline(Device& device, VkFormat colorFormat, VkFormat depthFormat, const std::vector<Uniform*>& uniforms, bool wireframe, const std::string& vertFile, const std::string& fragFile)
		: device(device)
		, isWireframe(wireframe)
		, mVertPath(ShaderDir + vertFile)
		, mFragPath(ShaderDir + fragFile)
		, mTescPath("")
		, mTesePath("")
        , mColorFormat(colorFormat)
        , mDepthFormat(depthFormat)
        , mUniforms(uniforms)
	{
		mSpvVertPath = SpvDir + vertFile + ".spv";
		mSpvFragPath = SpvDir + fragFile + ".spv";

		CreatePipelineLayout(uniforms);
		CreatePipeline();
	}

	Pipeline::Pipeline(Device& device, VkFormat colorFormat, VkFormat depthFormat, const std::vector<Uniform*>& uniforms, bool wireframe, const std::string& vertFile, const std::string& fragFile, const std::string& tescFile, const std::string& teseFile)
		: device(device)
		, isWireframe(wireframe)
		, mVertPath(ShaderDir + vertFile)
		, mFragPath(ShaderDir + fragFile)
		, mTescPath(ShaderDir + tescFile)
		, mTesePath(ShaderDir + teseFile)
		, mColorFormat(colorFormat)
		, mDepthFormat(depthFormat)
		, mUniforms(uniforms)
	{
		mSpvVertPath = SpvDir + vertFile + ".spv";
		mSpvFragPath = SpvDir + fragFile + ".spv";
		mSpvTescPath = SpvDir + tescFile + ".spv";
		mSpvTesePath = SpvDir + teseFile + ".spv";

		CreatePipelineLayout(uniforms);
		CreatePipeline();
	}

	void Pipeline::DestroyPipeline()
	{
		//Destroy shaders
		vkDestroyShaderModule(device.GetDevice(), mVertShaderModule, nullptr);
		vkDestroyShaderModule(device.GetDevice(), mFragShaderModule, nullptr);

		//Destroy pipeline
		vkDestroyPipeline(device.GetDevice(), mGraphicsPipeline, nullptr);
	}

	void Pipeline::Recreate()
	{
        vkDeviceWaitIdle(device.GetDevice());
		DestroyPipeline();
		
		if (!mVertPath.empty()) Shader::CompileShader(mVertPath);
        if (!mFragPath.empty()) Shader::CompileShader(mFragPath);
        if (!mTescPath.empty()) Shader::CompileShader(mTescPath);
        if (!mTesePath.empty()) Shader::CompileShader(mTesePath);

        CreatePipeline();
	}

	Pipeline::~Pipeline()
	{
		DestroyPipeline();
		vkDestroyPipelineLayout(device.GetDevice(), mPipelineLayout, nullptr);
	}

	void Pipeline::Bind(VkCommandBuffer commandBuffer)
	{
		//Bind the pipeline               vvv Tells it that this is a graphics pipeline (as apposed to a compute or raytracing pipeline)
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);
	}

	void Pipeline::CreatePipelineLayout(const std::vector<Uniform*>& uniforms)
	{
		//Setup push constant range if needed
		VkPushConstantRange pushConstantRange{};

		//Will hold create info for this layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};

		//Set what will be created to a pipeline layout
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		//Set uniform binding indexes and make vector of discriptions
		std::vector<VkDescriptorSetLayout> uniformsDescriptorSetLayouts;
		for (int i = 0; i < uniforms.size(); ++i)
		{
			uniforms[i]->SetBinding(i);
			uniformsDescriptorSetLayouts.push_back(uniforms[i]->GetDescriptorLayout()->GetDescriptorSetLayout());
		}
		//
		////Give the pipeline info about the uniform buffer
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(uniformsDescriptorSetLayouts.size()); //Set how many layouts are being provided
		pipelineLayoutCreateInfo.pSetLayouts = uniformsDescriptorSetLayouts.data();                           //Provide layouts
        
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0;

		//Create pipeline
		if (vkCreatePipelineLayout(device.GetDevice(), &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
		{
			//If failed throw error
            DOG_CRITICAL("Failed to create pipeline layout");
		}
	}

	void Pipeline::CreatePipeline()
	{
		//Make sure everything needed exists
		if (mPipelineLayout == nullptr)
		{
            DOG_CRITICAL("Cannot create pipeline before pipeline layout");
		}

		//Get a defualt pipeline configuration
		PipelineConfigInfo pipelineConfig{};
		Pipeline::DefaultPipelineConfigInfo(pipelineConfig);

		//Set wireframe mode
		pipelineConfig.rasterizationCreateInfo.polygonMode = isWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

		//Set topology
		pipelineConfig.inputAssemblyCreateInfo.topology = isWireframe ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		//Set renderpass
		pipelineConfig.renderPass = VK_NULL_HANDLE; //renderPass;

		//Set layout
		pipelineConfig.pipeLineLayout = mPipelineLayout;

		//Formats
        pipelineConfig.colorFormat = mColorFormat;
        pipelineConfig.depthFormat = mDepthFormat;

		//Create the pipeline
		CreateGraphicsPipeline(pipelineConfig);
	}

	void Pipeline::DefaultPipelineConfigInfo(PipelineConfigInfo& configInfo)
	{
		configInfo.inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		configInfo.inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		configInfo.inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
		configInfo.inputAssemblyCreateInfo.pNext = nullptr;
		configInfo.inputAssemblyCreateInfo.flags = 0;

		configInfo.viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		configInfo.viewportCreateInfo.viewportCount = 1;                                            
		configInfo.viewportCreateInfo.pViewports = nullptr;                                         
		configInfo.viewportCreateInfo.scissorCount = 1;                                             
		configInfo.viewportCreateInfo.pScissors = nullptr;                                          

		configInfo.rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		configInfo.rasterizationCreateInfo.depthClampEnable = VK_FALSE;
		configInfo.rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		configInfo.rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		configInfo.rasterizationCreateInfo.lineWidth = 1.0f;
		configInfo.rasterizationCreateInfo.cullMode = VK_CULL_MODE_NONE;
		configInfo.rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		configInfo.rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
		configInfo.rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
		configInfo.rasterizationCreateInfo.depthBiasClamp = 0.0f;
		configInfo.rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;


		configInfo.multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		configInfo.multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
		configInfo.multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		configInfo.multisampleCreateInfo.minSampleShading = 1.0f;
		configInfo.multisampleCreateInfo.pSampleMask = nullptr;
		configInfo.multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
		configInfo.multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

		//-----------------------------------------------------------------------------------------------------------------------------------
		//|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		//Color Blend Attachment/Info (Setting for how overlapping colors are blended)-------------------------------------------------------

		//Attachment-------------------

		//Sets which colors/alpha are allowed to be blended
		configInfo.colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		//Sets if blending is enabled
		configInfo.colorBlendAttachment.blendEnable = VK_TRUE;

		//Factor of how much of the source color/alpha is used
		configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

		//Factor of how much of the destination color/alpha is used
		configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

		//What operation to use when blending colors/alpha
		configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		//Create info-------------------

		//Sets what will be created to a color blender
		configInfo.colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

		//Sets if the set operations are used when blending
		configInfo.colorBlendCreateInfo.logicOpEnable = VK_FALSE;

		//Selects which operation to use
		configInfo.colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;

		//Number of attachments to use in this blender
		configInfo.colorBlendCreateInfo.attachmentCount = 1;

		//The attachments to be used
		configInfo.colorBlendCreateInfo.pAttachments = &configInfo.colorBlendAttachment;

		//Constants to be used when blending for each channel
		configInfo.colorBlendCreateInfo.blendConstants[0] = 0.0f;
		configInfo.colorBlendCreateInfo.blendConstants[1] = 0.0f;
		configInfo.colorBlendCreateInfo.blendConstants[2] = 0.0f;
		configInfo.colorBlendCreateInfo.blendConstants[3] = 0.0f;

		//-----------------------------------------------------------------------------------------------------------------------------------
		//|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		//Depth Buffer (Setup for depth storage)---------------------------------------------------------------------------------------------

		//Set what will be created to a depth buffer
		configInfo.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

		//Sets if depth testing is enabled
		configInfo.depthStencilCreateInfo.depthTestEnable = VK_TRUE;

		//Sets to writing to the depth buffer is enabled
		configInfo.depthStencilCreateInfo.depthWriteEnable = VK_TRUE;

		//Sets the operation to be used when comparing a new entry to the depth buffer compared to what is already there
		configInfo.depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;

		//Sets if bounds testing is enabled
		configInfo.depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;

		//Sets the minimum and maximum depth for the depth bounds test
		configInfo.depthStencilCreateInfo.minDepthBounds = 0.0f;
		configInfo.depthStencilCreateInfo.maxDepthBounds = 1.0f;

		//Sets if stencil testing is used (I dont currently know what that is)
		configInfo.depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

		//Sets the front and back for the stencil test
		configInfo.depthStencilCreateInfo.front = {};
		configInfo.depthStencilCreateInfo.back = {};

		//-----------------------------------------------------------------------------------------------------------------------------------
		//|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		//Dynamic State Create info (Configures Pipeline to expect dynamic scissor/viewport to be provided later)----------------------------

		//Set so viewport and scissor are expected to be dynamic
		configInfo.dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		configInfo.dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;                     //Define this struture to be a dynamic state create info
		configInfo.dynamicStateCreateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();                           //Provide what will be dynamic
		configInfo.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());	//Provide the number of things that will be dynamic
		configInfo.dynamicStateCreateInfo.flags = 0;																																				//No flags being used
		configInfo.dynamicStateCreateInfo.pNext = nullptr;																																	//No next

		//-----------------------------------------------------------------------------------------------------------------------------------
		//|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		//-----------------------------------------------------------------------------------------------------------------------------------

		
	}

	void Pipeline::CreateGraphicsPipeline(const PipelineConfigInfo& configInfo)
	{
		//Make sure layout and renderpass are real
		if (configInfo.pipeLineLayout == VK_NULL_HANDLE)
		{
            DOG_CRITICAL("Cannot create graphics pipeline: no pipelineLayout provided in configInfo");
		}

		// Read code files
		std::ifstream vertSPVFile(mSpvVertPath, std::ios::binary);
		std::ifstream fragSPVFile(mSpvFragPath, std::ios::binary);

		if (vertSPVFile.is_open() && fragSPVFile.is_open())
		{
			// read spv files
			vertSPVFile.seekg(0, std::ios::end);
			size_t vertSPVFileSize = vertSPVFile.tellg();
			vertSPVFile.seekg(0, std::ios::beg);

			std::vector<uint32_t> vertShaderSPV(vertSPVFileSize / sizeof(uint32_t));
			vertSPVFile.read(reinterpret_cast<char*>(vertShaderSPV.data()), vertSPVFileSize);
			vertSPVFile.close();

			fragSPVFile.seekg(0, std::ios::end);
			size_t fragSPVFileSize = fragSPVFile.tellg();
			fragSPVFile.seekg(0, std::ios::beg);

			std::vector<uint32_t> fragShaderSPV(fragSPVFileSize / sizeof(uint32_t));
			fragSPVFile.read(reinterpret_cast<char*>(fragShaderSPV.data()), fragSPVFileSize);
			fragSPVFile.close();

			Shader::CreateShaderModule(device, vertShaderSPV, &mVertShaderModule);
			Shader::CreateShaderModule(device, fragShaderSPV, &mFragShaderModule);
		}

		//Make create infos for vertex and fragment shader stages
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.push_back({});
		shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; //Set what will be created to a shader module
		shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;													 //Set type to vertex shader
		shaderStages[0].module = mVertShaderModule;																	 //Vertex shader to use
		shaderStages[0].pName = "main";																							 //Name of entry function in vertex shader
		shaderStages[0].flags = 0;																							     //Using no flags
		shaderStages[0].pNext = nullptr;																						 //Curently unsure what these two are used, but not currently using
		shaderStages[0].pSpecializationInfo = nullptr;															 //Curently unsure what these two are used, but not currently using

		shaderStages.push_back({});
		shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; //Same as above but for fragment shader
		shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages[1].module = mFragShaderModule;
		shaderStages[1].pName = "main";
		shaderStages[1].flags = 0;
		shaderStages[1].pNext = nullptr;
		shaderStages[1].pSpecializationInfo = nullptr;

		//Make create info for vertex input (data to be drawn input)

		//VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
		//vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		//vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
		//vertexInputCreateInfo.pVertexBindingDescriptions = nullptr; // Optional
		//vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
		//vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr; // Optional

		auto vertBindingDescriptions = SimpleVertex::GetBindingDescriptions();
		auto vertAttributeDescriptions = SimpleVertex::GetAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;                     //Set what will be crated to a vertex input
		vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertAttributeDescriptions.size()); //Counts for attribute desciptions of vertex buffers
		vertexInputCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertBindingDescriptions.size());     //Counts for binding  desciptions of vertex buffers
		vertexInputCreateInfo.pVertexAttributeDescriptions = vertAttributeDescriptions.data();                           //Attribute desciptions of vertex buffers
		vertexInputCreateInfo.pVertexBindingDescriptions = vertBindingDescriptions.data();                               //Binding desciptions of vertex buffers
		
		//Create pipeline object using everything we have set up
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;		//Set what will be created to a graphics pipeline
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());													//Count of programable shader stages being used
		pipelineCreateInfo.pStages = shaderStages.data();															//Programable shader stages to use
		pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;								//Set vertex input data

		pipelineCreateInfo.pInputAssemblyState = &configInfo.inputAssemblyCreateInfo; //Set data from config
		pipelineCreateInfo.pViewportState = &configInfo.viewportCreateInfo;					  //Set data from config
		pipelineCreateInfo.pRasterizationState = &configInfo.rasterizationCreateInfo; //Set data from config
		pipelineCreateInfo.pMultisampleState = &configInfo.multisampleCreateInfo;     //Set data from config
		pipelineCreateInfo.pColorBlendState = &configInfo.colorBlendCreateInfo;				//Set data from config
		pipelineCreateInfo.pDepthStencilState = &configInfo.depthStencilCreateInfo;   //Set data from config

		//If heightmap pipeline
		//VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo{};
		//if (isHeightmap)
		//{
		//	//Add a tessellation state to the pipeline
		//	pipelineTessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
		//	pipelineTessellationStateCreateInfo.patchControlPoints = 4;
		//	pipelineCreateInfo.pTessellationState = &pipelineTessellationStateCreateInfo;
		//}

		// Provide information for dynamic rendering
		VkPipelineRenderingCreateInfoKHR pipeline_create{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
		pipeline_create.pNext = VK_NULL_HANDLE;
		pipeline_create.colorAttachmentCount = 1;

		VkFormat uNormalFormat = device.GetLinearFormat();
		pipeline_create.pColorAttachmentFormats = &uNormalFormat;// &configInfo.colorFormat;
        pipeline_create.depthAttachmentFormat = configInfo.depthFormat;
        pipeline_create.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

		pipelineCreateInfo.pDynamicState = &configInfo.dynamicStateCreateInfo;				//Provides create information for what should be dynamic (changable after creation)

		pipelineCreateInfo.layout = configInfo.pipeLineLayout;												//Set data from config
		pipelineCreateInfo.renderPass = VK_NULL_HANDLE;// configInfo.renderPass;												//Set data from config
		pipelineCreateInfo.subpass = configInfo.subpass;															//Set data from config

		pipelineCreateInfo.basePipelineIndex = -1;                                    //Used for optimazion by deriving new pipelines from existing ones (not currently using)
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;												//Used for optimazion by deriving new pipelines from existing ones (not currently using)

        pipelineCreateInfo.pNext = &pipeline_create; // Link the dynamic rendering info to the pipeline create info

		//Create the pipeline          not using pipeline cashe vvvvvvvvvv     no allocation callbacks vvvvv
		if (vkCreateGraphicsPipelines(device.GetDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS)
		{
            DOG_ERROR("Failed to create graphics pipeline");
		}
	}
}

