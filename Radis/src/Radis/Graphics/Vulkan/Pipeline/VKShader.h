/*****************************************************************//**
 * \file   VKShader.h
 * \brief  Definition of the Shader class for Vulkan shader compilation and module creation.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
	class Device;

	struct Shader
	{
		static bool CompileShader(const std::string& shaderPath);
		static void CreateShaderModule(Device& device, const std::vector<uint32_t>& code, VkShaderModule* shaderModule);
	};
}
