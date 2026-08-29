#pragma once

#include "PoGlfw.h"
#include <vector>
#include "imgui.h"
#include <string>
struct ImGuiContext;


struct SPoGuiSettings
{

};

struct SPoGuiState
{
	VkDevice mDevice = VK_NULL_HANDLE;
	VkCommandPool mCommandPool = VK_NULL_HANDLE;
	int mImageCount = 0;
};

struct SPoGuiResources
{
	VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> mCommandBuffers = {};
	ImGuiContext *mpContext = nullptr;
};

namespace NPoGuiBehavior
{
	VkCommandBuffer draw(SPoGuiResources &inOutResources, VkImage swapChainResolveImage, VkImage swapChainColorImage, VkImageView swapChainResolveImageView, VkImageView swapChainColorImageView, VkImageView swapChainDepthImageView, VkExtent2D const extent, VkFormat const colorFormat, int const imageIndex);

	SPoGuiState init(SPoGuiResources &outResources, SPoGuiSettings const &settings, VkDevice device, int const width, int const height, GLFWwindow *pWindow, VkFormat const colorFormat,
		uint32_t const apiVersion, VkInstance instance, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, uint32_t const queueFamily, VkQueue queue, uint32_t const minImageCount, uint32_t const imageCount, VkSampleCountFlagBits const msaaSamples,
		VkPipelineCache pipelineCache, VkFormat const depthFormat);
	void cleanup(SPoGuiResources &inOutResources, SPoGuiState &inOutState, SPoGuiSettings const &settings);
}