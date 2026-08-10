#pragma once

#include "PoGlfw.h"


#include <string>


struct SPoVulkanShaderSettings
{
	std::string mPath;
	VkShaderStageFlagBits mStageFlags;
	std::string mEntryName = "main";
};

struct SPoVulkanShaderState
{
	VkDevice mDevice = VK_NULL_HANDLE;
};

struct SPoVulkanShaderResources
{
	VkShaderModule mShaderModule = VK_NULL_HANDLE;

};

namespace NPoVulkanShaderBehavior
{
	SPoVulkanShaderState init(SPoVulkanShaderResources &outResources, SPoVulkanShaderSettings const &settings, VkDevice device);

	void cleanup(SPoVulkanShaderResources &inOutResources, SPoVulkanShaderState &inOutState, SPoVulkanShaderSettings const &settings);
}
