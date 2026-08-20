#include "PoVulkan.h"
#include "PoGlfw.h"

#include <stdexcept>
#include <iostream>
#include "slang/slang.h"

namespace NPoVulkanPrivate
{
	bool check_validation_layer_support(SPoVulkanSettings const &settings)
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char *layerName : settings.mValidationLayers)
		{
			bool found = false;
			for (const auto &layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				return false;
			}
		}

		return true;
	}

	std::vector<char const *> get_required_extensions(SPoVulkanSettings const &settings)
	{
		uint32_t glfwExtensionCount = 0;
		char const **glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<char const *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (settings.mIsValidationLayersEnabled)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		return extensions;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		VkDebugUtilsMessengerCallbackDataEXT const *pCallbackData,
		void *pUserData)
	{
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debug_callback;
	}

	void verify_extensions(std::vector<char const *> glfwExtensions)
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);

		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::cout << "available extensions:\n";

		for (const auto &extension : extensions)
		{
			std::cout << '\t' << extension.extensionName << '\n';

		}

		std::vector<const char *> missingExtensions;
		for (const char *requiredExtension : glfwExtensions)
		{
			bool found = false;
			for (const auto &extension : extensions)
			{
				if (_strcmpi(extension.extensionName, requiredExtension) == 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				missingExtensions.push_back(requiredExtension);
			}
		}
		if (missingExtensions.size() > 0)
		{
			std::string errorMessage = "Missing the following extensions : \n";
			for (const auto &extension : missingExtensions)
			{
				errorMessage.push_back('\t');
				errorMessage.append(extension);
				errorMessage.push_back('\n');
			}
			errorMessage.push_back('\0');
			throw std::runtime_error(errorMessage.c_str());
		}
	}

	void init_instance(VkInstance &outInstance, SPoVulkanSettings const &settings)
	{
		using namespace NPoVulkanPrivate;
		if (settings.mIsValidationLayersEnabled && !check_validation_layer_support(settings))
		{
			throw std::runtime_error("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = settings.mName.c_str();
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_4;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		std::vector<char const *> extensions = get_required_extensions(settings);
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (settings.mIsValidationLayersEnabled)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(settings.mValidationLayers.size());
			createInfo.ppEnabledLayerNames = settings.mValidationLayers.data();

			populate_debug_messenger_create_info(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		verify_extensions(extensions);

		if (vkCreateInstance(&createInfo, nullptr, &outInstance) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create instance!");
		}
	}
}

namespace NPoVulkanPrivate
{
	void init_debug_messenger(VkDebugUtilsMessengerEXT &outDebugMessenger, VkInstance const instance, SPoVulkanSettings const &settings)
	{
		if (!settings.mIsValidationLayersEnabled)
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		populate_debug_messenger_create_info(createInfo);

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			if (func(instance, &createInfo, nullptr, &outDebugMessenger) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to setup debug messenger!");
			}
		}
	}
}

SPoVulkanState NPoVulkanBehavior::init(SPoVulkanResources &outResources, SPoVulkanSettings const &settings)
{
	using namespace NPoVulkanPrivate;
	SPoVulkanState state;
	// TODO : We have to rethink slang dependency and vulkan dependency on slang they are bidirectional right now which is super gross
	// 
	init_instance(outResources.mInstance, settings);
	if (settings.mIsValidationLayersEnabled)
	{
		init_debug_messenger(outResources.mDebugMessenger, outResources.mInstance, settings);
	}

	return state;
}


void NPoVulkanBehavior::cleanup(SPoVulkanResources &inOutResources, SPoVulkanState &inOutState, SPoVulkanSettings const &settings)
{

	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inOutResources.mInstance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(inOutResources.mInstance, inOutResources.mDebugMessenger, nullptr);
	}
	vkDestroyInstance(inOutResources.mInstance, nullptr);

	inOutResources = {};
	inOutState = {};
}