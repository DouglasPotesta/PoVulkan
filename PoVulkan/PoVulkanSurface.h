#pragma once

#include "PoGlfw.h"
#include "PoVulkanSwapchain.h"

#include <vector>


struct SPoVulkanSurfaceSettings
{
	std::vector<const char *> mDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	SPoVulkanSwapchainSettings mSwapchain;
};

struct SPoVulkanSurfaceState
{
	VkInstance mInstance = VK_NULL_HANDLE;
	GLFWwindow *mpWindow = nullptr;

	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	SQueueFamilyIndices mQueueFamilyIndices = {};
	SSwapchainSupportDetails mSwapchainSupportDetails = {};
	// note this is managed by the logical device
	VkQueue mGraphicsQueue;
	// note this is managed by the logical device
	VkQueue mPresentQueue;

	SPoVulkanSwapchainState mSwapchain;

	VkSampleCountFlagBits mMsaaCount = VK_SAMPLE_COUNT_1_BIT;
	VkPhysicalDeviceMemoryProperties mMemoryProperties;
	VkSurfaceFormatKHR mSurfaceFormat = {};
	VkFormat mDepthFormat = {};
};

struct SPoVulkanSurfaceResources
{
	VkSurfaceKHR mSurface = VK_NULL_HANDLE;
	VkDevice mLogicalDevice = VK_NULL_HANDLE;
	SPoVulkanSwapchainResources mSwapchain;
	VkRenderPass mRenderPass = VK_NULL_HANDLE;
	VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
	VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;
	VkCommandPool mCommandPool = VK_NULL_HANDLE;
};

namespace NPoVulkanSurfaceBehavior
{
	SQueueFamilyIndices find_queue_families(VkSurfaceKHR surface, VkPhysicalDevice device);

	void create_image(VkImage &outImage, VkDeviceMemory &outImageMemory, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkPhysicalDeviceMemoryProperties const &memoryProperties);
	VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void transition_image_layout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

	SPoVulkanSurfaceState init(SPoVulkanSurfaceResources &outResources, SPoVulkanSurfaceSettings const &settings, VkInstance instance, GLFWwindow *pWindow);
	void cleanup(SPoVulkanSurfaceResources &inOutResources, SPoVulkanSurfaceState &inOutState, SPoVulkanSurfaceSettings const &settings);
}