
#include "PoWindow.h"

static void recreate_swap_chains_for_resize(SPoWindowResources &inOutResources, SPoWindowState &inOutState, SPoWindowSettings const &settings)
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(inOutResources.mpWindow, &width, &height);

	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(inOutResources.mpWindow, &width, &height);
		glfwWaitEvents();
	}
	NPoVulkanSwapchainBehavior::cleanup(inOutResources.mDevice.mSwapchain, inOutState.mDevice.mSwapchain, settings.mDevice.mSwapchain);

	VkExtent2D const &extent = NPoVulkanSwapchainBehavior::choose_swap_extent(inOutResources.mpWindow, inOutState.mDevice.mSwapchainSupportDetails.mCapabilities);
	inOutState.mDevice.mSwapchain = NPoVulkanSwapchainBehavior::init(
		inOutResources.mDevice.mSwapchain, inOutResources.mDevice.mSurface, inOutResources.mDevice.mLogicalDevice, inOutResources.mDevice.mCommandPool, inOutState.mDevice.mGraphicsQueue, inOutResources.mDevice.mRenderPass,
		settings.mDevice.mSwapchain, inOutState.mDevice.mSwapchainSupportDetails, inOutState.mDevice.mQueueFamilyIndices, 
		extent, inOutState.mDevice.mSurfaceFormat, inOutState.mDevice.mDepthFormat, inOutState.mDevice.mMsaaCount, inOutState.mDevice.mMemoryProperties);
	
}

static void framebufferResizeCallback(GLFWwindow *pWindow, int width, int height)
{
	if (void *pPointer = glfwGetWindowUserPointer(pWindow))
	{
		SPoWindowResizeCommand &command = *reinterpret_cast<SPoWindowResizeCommand *>(pPointer);

		recreate_swap_chains_for_resize(*(command.mpResources), *(command.mpState), *(command.mpSettings));
	}
}

SPoWindowState NPoWindowBehavior::init(SPoWindowResources &inOutResources, SPoWindowSettings const &settings, VkInstance vulkanInstance)
{
	SPoWindowState state;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	inOutResources.mpWindow = glfwCreateWindow(settings.mWidth, settings.mHeight, settings.mName.c_str(), nullptr, nullptr);
	
	glfwSetFramebufferSizeCallback(inOutResources.mpWindow, framebufferResizeCallback);
	state.mVulkanInstance = vulkanInstance;



	state.mDevice = NPoVulkanDeviceBehavior::init(inOutResources.mDevice, settings.mDevice, vulkanInstance, inOutResources.mpWindow);



	return state;
}

void NPoWindowBehavior::cleanup(SPoWindowResources &inOutResources, SPoWindowState &inOutState, SPoWindowSettings const &settings)
{
	NPoVulkanDeviceBehavior::cleanup(inOutResources.mDevice, inOutState.mDevice, settings.mDevice);

	glfwDestroyWindow(inOutResources.mpWindow);
	inOutResources.mpWindow = nullptr;
}