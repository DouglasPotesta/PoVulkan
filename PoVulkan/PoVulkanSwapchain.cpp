#include "PoVulkanSwapchain.h"
#include "PoVulkanDevice.h"

#include <stdexcept>
#include <algorithm>
#include <array>

SSwapchainSupportDetails NPoVulkanSwapchainBehavior::query_swap_chain_support(VkSurfaceKHR surface, VkPhysicalDevice device)
{
	SSwapchainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.mCapabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.mFormats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.mFormats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.mPresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.mPresentModes.data());
	}

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.mCapabilities);

	return details;
}



VkExtent2D NPoVulkanSwapchainBehavior::choose_swap_extent(GLFWwindow *pWindow, VkSurfaceCapabilitiesKHR const &capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(pWindow, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return actualExtent;
	}
}


VkSurfaceFormatKHR NPoVulkanSwapchainBehavior::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
	for (const auto &availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}
	// could do some special scoring to pick best available if desired not available, but this good for now.
	return availableFormats[0];
}


namespace NPoVulkanSwapchainPrivate
{



	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR> &availablePresentModes)
	{
		for (const auto &availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	void create_swap_chain(VkSwapchainKHR &outSwapchain, SPoVulkanSwapchainState const &state, SSwapchainSupportDetails const &swapChainSupport, SQueueFamilyIndices const &indices, VkSurfaceFormatKHR const surfaceFormat)
	{
		using namespace NPoVulkanSwapchainBehavior;

		uint32_t imageCount = swapChainSupport.mCapabilities.minImageCount + 1;

		if (swapChainSupport.mCapabilities.maxImageCount > 0 && imageCount > swapChainSupport.mCapabilities.maxImageCount)
		{
			imageCount = swapChainSupport.mCapabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = state.mSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = state.mExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// TODO : make this an argument and migrate the find_queue_families function back to private surface
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}
		createInfo.preTransform = swapChainSupport.mCapabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = state.mPresentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(state.mDevice, &createInfo, nullptr, &outSwapchain) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain!");
		}
	}

	void create_color_resources(VkImage &outColorImage, VkDeviceMemory &outColorImageMemory, VkImageView &outColorImageView,
		VkDevice logicalDevice,
		VkFormat const format, VkSampleCountFlagBits const msaaSamples, VkExtent2D const &extent, VkPhysicalDeviceMemoryProperties const &memoryProperties)
	{
		VkFormat colorFormat = format;

		NPoVulkanDeviceBehavior::create_image(outColorImage, outColorImageMemory, logicalDevice,
			extent.width, extent.height, 1, msaaSamples, format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryProperties);
		outColorImageView = NPoVulkanDeviceBehavior::create_image_view(logicalDevice, outColorImage, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

	void create_depth_resources(VkImage &outDepthImage, VkDeviceMemory &outDepthImageMemory, VkImageView &outDepthImageView,
		VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
		VkFormat const format, VkSampleCountFlagBits const msaaSamples, VkExtent2D const &extent, VkPhysicalDeviceMemoryProperties const &memoryProperties)
	{
		NPoVulkanDeviceBehavior::create_image(outDepthImage, outDepthImageMemory, logicalDevice,
			extent.width, extent.height, 1, msaaSamples,
			format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryProperties);
		outDepthImageView = NPoVulkanDeviceBehavior::create_image_view(logicalDevice, outDepthImage, format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

		NPoVulkanDeviceBehavior::transition_image_layout(logicalDevice, commandPool, graphicsQueue, outDepthImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
	}


	void create_frame_buffers(std::vector<VkFramebuffer> &outFrameBuffers, VkDevice device, std::vector<VkImageView> const &imageViews, VkImageView colorImageView, VkImageView depthImageView, VkRenderPass renderPass, VkExtent2D const &extent)
	{
		outFrameBuffers.resize(imageViews.size());

		for (size_t i = 0; i < imageViews.size(); ++i)
		{
			std::array<VkImageView, 3> attachments = {
				colorImageView,
				depthImageView,
				imageViews[i]
			};
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = extent.width;
			framebufferInfo.height = extent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &outFrameBuffers[i]) != VK_FALSE)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}
}



SPoVulkanSwapchainState NPoVulkanSwapchainBehavior::init(SPoVulkanSwapchainResources &outResources, VkSurfaceKHR surface, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkRenderPass renderPass,
	SPoVulkanSwapchainSettings const &settings, SSwapchainSupportDetails const &supportDetails, SQueueFamilyIndices const &indices, 
	VkExtent2D const &extent, VkSurfaceFormatKHR const surfaceFormat, VkFormat const depthFormat, VkSampleCountFlagBits const msaaCount, VkPhysicalDeviceMemoryProperties const &memoryProperties)
{
	using namespace NPoVulkanSwapchainPrivate;
	SPoVulkanSwapchainState state;
	state.mDevice = device;
	state.mSurface = surface;
	state.mExtent = extent;
	state.mPresentMode = choose_swap_present_mode(supportDetails.mPresentModes);

	create_swap_chain(outResources.mSwapchain, state, supportDetails, indices, surfaceFormat);
	
	vkGetSwapchainImagesKHR(device, outResources.mSwapchain, &(state.mImageCount), nullptr);
	outResources.mImages.resize(state.mImageCount);
	vkGetSwapchainImagesKHR(device, outResources.mSwapchain, &(state.mImageCount), outResources.mImages.data());

	outResources.mSwapchainImageViews.resize(state.mImageCount);
	for (size_t i = 0; i < state.mImageCount; ++i)
	{
		outResources.mSwapchainImageViews[i] = NPoVulkanDeviceBehavior::create_image_view(device, outResources.mImages[i], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

	create_color_resources(outResources.mColorImage, outResources.mColorImageMemory, outResources.mColorImageView, device,
		surfaceFormat.format, msaaCount, extent, memoryProperties);
	create_depth_resources(outResources.mDepthImage, outResources.mDepthImageMemory, outResources.mDepthImageView, device, commandPool, graphicsQueue,
		depthFormat, msaaCount, extent, memoryProperties);

	create_frame_buffers(outResources.mFrameBuffers, device, outResources.mSwapchainImageViews, outResources.mColorImageView, outResources.mDepthImageView, renderPass, extent);
    return state;
}

void NPoVulkanSwapchainBehavior::cleanup(SPoVulkanSwapchainResources &inOutResources, SPoVulkanSwapchainState &inOutState, SPoVulkanSwapchainSettings const &settings)
{
	vkDestroyImageView(inOutState.mDevice, inOutResources.mColorImageView, nullptr);
	vkDestroyImage(inOutState.mDevice, inOutResources.mColorImage, nullptr);
	vkFreeMemory(inOutState.mDevice, inOutResources.mColorImageMemory, nullptr);

	vkDestroyImageView(inOutState.mDevice, inOutResources.mDepthImageView, nullptr);
	vkDestroyImage(inOutState.mDevice, inOutResources.mDepthImage, nullptr);
	vkFreeMemory(inOutState.mDevice, inOutResources.mDepthImageMemory, nullptr);


	for (auto framebuffer : inOutResources.mFrameBuffers)
	{
		vkDestroyFramebuffer(inOutState.mDevice, framebuffer, nullptr);
	}

	for (auto imageView : inOutResources.mSwapchainImageViews)
	{
		vkDestroyImageView(inOutState.mDevice, imageView, nullptr);
	}
	vkDestroySwapchainKHR(inOutState.mDevice, inOutResources.mSwapchain, nullptr);
	inOutResources = {};
	inOutState = {};
}
