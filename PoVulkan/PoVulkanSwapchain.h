#pragma once

#include "PoGlfw.h"

#include <optional>
#include <vector>



struct SSwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR mCapabilities;
	std::vector<VkSurfaceFormatKHR> mFormats;
	std::vector<VkPresentModeKHR> mPresentModes;
};

struct SQueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};


struct SPoVulkanSwapchainSettings
{
};

struct SPoVulkanSwapchainState
{
	// TODO : Evaluate if these are at all needed outside of initialization
	//SSwapchainSupportDetails mSupportDetails = {};
	//SQueueFamilyIndices mQueueFamilyIndices = {};
	VkSurfaceKHR mSurface = VK_NULL_HANDLE;
	VkDevice mDevice = VK_NULL_HANDLE;
	VkExtent2D mExtent = {};
	VkPresentModeKHR mPresentMode = {};
	uint32_t mImageCount = 0;
};

struct SPoVulkanSwapchainResources
{
	VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
	std::vector<VkImage> mImages;
	std::vector<VkImageView> mSwapchainImageViews;
	VkImage mColorImage = VK_NULL_HANDLE;
	VkDeviceMemory mColorImageMemory = VK_NULL_HANDLE;
	VkImageView mColorImageView = VK_NULL_HANDLE;
	VkImage mDepthImage = VK_NULL_HANDLE;
	VkDeviceMemory mDepthImageMemory = VK_NULL_HANDLE;
	VkImageView mDepthImageView = VK_NULL_HANDLE;
};

namespace NPoVulkanSwapchainBehavior
{
	SSwapchainSupportDetails query_swap_chain_support(VkSurfaceKHR surface, VkPhysicalDevice device);
	VkExtent2D choose_swap_extent(GLFWwindow *pWindow, VkSurfaceCapabilitiesKHR const &capabilities);
	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &availableFormats);
	SPoVulkanSwapchainState init(SPoVulkanSwapchainResources &outResources, VkSurfaceKHR surface, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
		SPoVulkanSwapchainSettings const &settings, SSwapchainSupportDetails const &supportDetails, SQueueFamilyIndices const &indices, 
		VkExtent2D const &extent, VkSurfaceFormatKHR const surfaceFormat, VkFormat const depthFormat, VkSampleCountFlagBits const msaaCount, VkPhysicalDeviceMemoryProperties const &memoryProperties);
	void cleanup(SPoVulkanSwapchainResources &inOutResources, SPoVulkanSwapchainState &inOutState, SPoVulkanSwapchainSettings const &settings);
}