#pragma once

#include "PoGlfw.h"
#include "PoVulkanSwapchain.h"

#include <vector>


struct SPoVulkanDeviceSettings
{
	std::vector<const char *> mDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	SPoVulkanSwapchainSettings mSwapchain;
};

struct SPoVulkanDeviceState
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

struct SPoVulkanDeviceResources
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

namespace NPoVulkanDeviceBehavior
{
	SQueueFamilyIndices find_queue_families(VkSurfaceKHR surface, VkPhysicalDevice device);

	void create_image(VkImage &outImage, VkDeviceMemory &outImageMemory, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkPhysicalDeviceMemoryProperties const &memoryProperties);
	VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void transition_image_layout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);


	void create_buffer(VkBuffer &inOutBuffer, VkDeviceMemory &inOutBufferMemory, VkDevice device, VkDeviceSize const size, VkBufferUsageFlags const usage, VkMemoryPropertyFlags const properties, VkPhysicalDeviceMemoryProperties const &memoryProperties);
	void copy_buffer_to_image(VkImage image, VkBuffer buffer, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, uint32_t const width, uint32_t const height);
	void generate_mipmaps(VkImage image, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
		VkFormat const imageFormat, int32_t const texWidth, int32_t const texHeight, uint32_t const mipLevels);
	void copy_buffer(VkBuffer dstBuffer, VkBuffer srcBuffer, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkDeviceSize const deviceSize);

	SPoVulkanDeviceState init(SPoVulkanDeviceResources &outResources, SPoVulkanDeviceSettings const &settings, VkInstance instance, GLFWwindow *pWindow);
	void cleanup(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings);
}