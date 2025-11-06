#include <PCH/pch.h>

#include "VKShader.h"
#include "../Core/Device.h"

#include "glslang/Public/ResourceLimits.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include "../Uniform/ShaderTypes.h"

#include "Engine.h"

namespace Dog
{
	bool Shader::CompileShader(const std::string& shaderPath)
	{
        namespace fs = std::filesystem;

        fs::path path(shaderPath);
        if (!fs::exists(path))
        {
            std::cerr << "Shader not found: " << shaderPath << "\n";
            return false;
        }

        // Skip already compiled .spv files
        if (path.extension() == ".spv")
        {
            std::cout << "Skipping precompiled SPIR-V file: " << path.filename().string() << "\n";
            return false;
        }

        // Create "spv" directory if missing
        fs::path spvDir = path.parent_path() / "spv";
        fs::create_directories(spvDir);

        // Output file = e.g. "Assets/Shaders/spv/myshader.vert.spv"z
        fs::path outputPath = spvDir / (path.filename().string() + ".spv");

        std::string pathStr = "Assets/shaders/" + path.filename().string();
        std::string outputPathStr = "Assets/shaders/spv/" + path.filename().string() + ".spv";

        // Build command line
        std::string glslangValidatorPath;
        if (Engine::IsDevBuild())
        {
            glslangValidatorPath = "..\\..\\Dependencies\\glslc\\glslangValidator.exe";
        }
        else
        {
            glslangValidatorPath = "glslangValidator.exe";
        }

        std::string command = std::format(
            "{} -V --target-env vulkan1.4 -Od -g --spirv-val \"{}\" -o \"{}\"", // No quotes here
            glslangValidatorPath, pathStr, outputPathStr
        );

        std::cout << "Command: " << command << "\n";

        int result = std::system(command.c_str());
        if (result != 0)
        {
            DOG_ERROR("Shader compilation failed: {}", path.filename().string());
            return false;
        }

        DOG_INFO("Shader compiled: {}", path.filename().string());
        return true;
	}

	void Shader::CreateShaderModule(Device& device, const std::vector<uint32_t>& code, VkShaderModule* shaderModule)
	{
		//Struct to hold information on how to create this shader module
		VkShaderModuleCreateInfo createInfo{};

		//Set type of object to create to shader module
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

		//Set the size of the passed code
		createInfo.codeSize = code.size() * sizeof(uint32_t);

		//Set pointer to code (cast from array of chars to a int32 pointer)
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		//Call create, what each parameter is: (gets Vulcan device handle, Create Info, no allication callbacks, Shader module to create)
		if (vkCreateShaderModule(device, &createInfo, nullptr, shaderModule) != VK_SUCCESS)
		{
			//Throw error if failed
			DOG_CRITICAL("Failed to create shader module");
		}
	}
}