#pragma once

#include "PoGlfw.h"
#include "PoVulkanDevice.h"


#include <vector>
#include <string>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#ifdef NDEBUG
#define PO_DEBUG_VALIDATION_LAYERS_ENABLED_DEFAULT (false)
#else
#define PO_DEBUG_VALIDATION_LAYERS_ENABLED_DEFAULT (true)
#endif


struct SPoVulkanSettings
{
	std::vector<const char *> mValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	std::string mName = "PoVulkan";

	bool mIsValidationLayersEnabled = PO_DEBUG_VALIDATION_LAYERS_ENABLED_DEFAULT;
};

struct SPoVulkanState
{
};

struct SPoVulkanResources
{
	VkInstance mInstance;
	VkDebugUtilsMessengerEXT mDebugMessenger;
	Slang::ComPtr<slang::IGlobalSession> mpSlangGlobalSession;
};

namespace NPoVulkanBehavior
{
	SPoVulkanState init(SPoVulkanResources &outResources, SPoVulkanSettings const &settings);
	void cleanup(SPoVulkanResources &outResources, SPoVulkanSettings const &settings);
}