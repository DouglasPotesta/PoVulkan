#pragma once

#include "PoGlfw.h"

#include <string>

// TODO : I am thinking I might want to rename these as PoVulkanTextures since they are very specific to vulkan implementation

// TODO : Move this to its own header file
enum class EPoMipmapCreationType : uint8_t
{
	None,
	Max

};

struct SPoTextureSettings
{
	std::string mPath;
	EPoMipmapCreationType  mMipMapCreationType = EPoMipmapCreationType::None;
};

struct SPoTextureState
{
	int mWidth = 0;
	int mHeight = 0;
	int mChannels = 0;
	VkDeviceSize mImageSize = 0;
	uint32_t mMipLevels = 0;
	VkDevice mDevice = VK_NULL_HANDLE;
};


struct SPoTextureResources
{
	VkImage mImage = VK_NULL_HANDLE;
	VkDeviceMemory mImageMemory = VK_NULL_HANDLE;
	VkImageView mImageView = VK_NULL_HANDLE;
	VkSampler mSampler = VK_NULL_HANDLE;
};

namespace NPoTextureBehavior
{
	VkSampler create_texture_sampler(VkDevice device, VkPhysicalDevice physicalDevice);

	SPoTextureState init(SPoTextureResources &outResources, SPoTextureSettings const &settings,
		VkDevice device, VkPhysicalDevice physicalDevice, VkBuffer stagingBuffer, VkDeviceMemory stagingBufferMemory, VkCommandPool commandPool, VkQueue graphicsQueue,
		VkBufferUsageFlags const usage, VkMemoryPropertyFlags const properties, VkPhysicalDeviceMemoryProperties const &memoryProperties); 

	void cleanup(SPoTextureResources &inOutResources, SPoTextureState &inOutState, SPoTextureSettings const &settings);
}