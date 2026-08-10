#include "PoVulkanShader.h"

#include <vector>
#include <string>
#include <ios>
#include <fstream>

namespace NPoVulkanShaderPrivate
{
	std::vector<char> read_file(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}

}


SPoVulkanShaderState NPoVulkanShaderBehavior::init(SPoVulkanShaderResources &outResources, SPoVulkanShaderSettings const &settings, VkDevice device)
{
	using namespace NPoVulkanShaderPrivate;
	SPoVulkanShaderState state = {};
	std::vector<char> code = read_file(settings.mPath);
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
	if (vkCreateShaderModule(device, &createInfo, nullptr, &outResources.mShaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}
    return state;
}

void NPoVulkanShaderBehavior::cleanup(SPoVulkanShaderResources &inOutResources, SPoVulkanShaderState &inOutState, SPoVulkanShaderSettings const &settings)
{
	vkDestroyShaderModule(inOutState.mDevice, inOutResources.mShaderModule, nullptr);
    inOutResources = {};
    inOutState = {};
}
