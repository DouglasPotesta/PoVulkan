#pragma once

#include "PoGlfw.h"
#include "PoVulkanSwapchain.h"
#include "PoGameObject.h"
#include "PoTexture.h"
#include "PoMesh.h"
#include "PoWindow.h"

#include <array>
#include <vector>


struct SPoWindowResizeCommand
{
	struct SPoVulkanDeviceSettings *mpSettings = nullptr;
	struct SPoVulkanDeviceResources *mpResources = nullptr;
	struct SPoVulkanDeviceState *mpState = nullptr;
};

struct SPoWindowId
{
	int mId = -1;

	static int sUniqueIdSeed;
	bool operator==(SPoWindowId const &other) { return other.mId == mId; }
};

struct SPoVulkanWindowSettings
{
	SPoWindowSettings mWindow;
	SPoVulkanSwapchainSettings mSwapchain;
	std::array<SPoGameObjectSettings, NPoGameObjectBehavior::gk_max_game_objects> mGameObjects;

};

struct SPoVulkanWindowState
{
	SPoWindowState mWindow;
	GLFWwindow *mpWindow = nullptr;
	std::array<SPoGameObjectState, NPoGameObjectBehavior::gk_max_game_objects> mGameObjects;
	SPoVulkanSwapchainState mSwapchain;
	int mCurrentFrame = 0;
	bool mFramebufferResized = false;
	SPoWindowId mUniqueId = {};
};

struct SPoVulkanWindowResources
{
	SPoWindowResources mWindow;
	VkSurfaceKHR mSurface = VK_NULL_HANDLE;
	SPoVulkanSwapchainResources mSwapchain;
	VkRenderPass mRenderPass = VK_NULL_HANDLE;
	VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
	VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;
	VkCommandPool mCommandPool = VK_NULL_HANDLE;
	VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> mCommandBuffers;
	std::vector<VkSemaphore> mImageAvailableSemaphores;
	std::vector<VkSemaphore> mRenderFinishedSemaphores;
	std::vector<VkFence> mInFlightFences;
	std::array<SPoGameObjectResources, NPoGameObjectBehavior::gk_max_game_objects> mGameObjects;
};

struct SPoVulkanDeviceSettings
{
	std::vector<const char *> mDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	SPoVulkanSwapchainSettings mSwapchain;
	std::array<SPoGameObjectSettings, NPoGameObjectBehavior::gk_max_game_objects> mGameObjects;

	SPoTextureSettings mTexture;
	SPoMeshSettings mMesh;

	SPoVulkanWindowSettings mWindowsSettings;
};

struct SPoVulkanDeviceState
{
	VkInstance mInstance = VK_NULL_HANDLE;

	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	SQueueFamilyIndices mQueueFamilyIndices = {};
	SSwapchainSupportDetails mSwapchainSupportDetails = {};
	// note this is managed by the logical device
	VkQueue mGraphicsQueue;
	// note this is managed by the logical device

	GLFWwindow *mpWindow = nullptr;
	VkQueue mPresentQueue;
	std::array<SPoGameObjectState, NPoGameObjectBehavior::gk_max_game_objects> mGameObjects;
	SPoVulkanSwapchainState mSwapchain;
	int mCurrentFrame = 0;
	bool mFramebufferResized = false;
	std::vector<SPoVulkanWindowState> mWindowStateVector;


	VkSampleCountFlagBits mMsaaCount = VK_SAMPLE_COUNT_1_BIT;
	VkPhysicalDeviceMemoryProperties mMemoryProperties;
	VkSurfaceFormatKHR mSurfaceFormat = {};
	VkFormat mDepthFormat = {};
	SPoTextureState mTexture;
	SPoMeshState mMesh;
};

struct SPoVulkanDeviceResources
{
	VkDevice mLogicalDevice = VK_NULL_HANDLE;
	SPoTextureResources mTexture;
	SPoMeshResources mMesh;

	std::array<SPoGameObjectResources, NPoGameObjectBehavior::gk_max_game_objects> mGameObjects;
	VkSurfaceKHR mSurface = VK_NULL_HANDLE;
	SPoVulkanSwapchainResources mSwapchain;
	VkRenderPass mRenderPass = VK_NULL_HANDLE;
	VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
	VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;
	VkCommandPool mCommandPool = VK_NULL_HANDLE;
	VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> mCommandBuffers;
	std::vector<VkSemaphore> mImageAvailableSemaphores;
	std::vector<VkSemaphore> mRenderFinishedSemaphores;
	std::vector<VkFence> mInFlightFences;
	std::vector<SPoVulkanWindowResources> mWindowResourcesVector;
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

	void draw_frame(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings);
	void recreate_swap_chains_for_resize(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings);

	SPoWindowId add_window(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings);
	void close_window(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings, SPoWindowId const windowId);
	void draw_window_frames(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings);
}