
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
	NPoVulkanSwapchainBehavior::cleanup(inOutResources.mSurface.mSwapchain, inOutState.mSurface.mSwapchain, settings.mSurface.mSwapchain);

	VkExtent2D const &extent = NPoVulkanSwapchainBehavior::choose_swap_extent(inOutResources.mpWindow, inOutState.mSurface.mSwapchainSupportDetails.mCapabilities);
	inOutState.mSurface.mSwapchain = NPoVulkanSwapchainBehavior::init(
		inOutResources.mSurface.mSwapchain, inOutResources.mSurface.mSurface, inOutResources.mSurface.mLogicalDevice, inOutResources.mSurface.mCommandPool, inOutState.mSurface.mGraphicsQueue, inOutResources.mSurface.mRenderPass,
		settings.mSurface.mSwapchain, inOutState.mSurface.mSwapchainSupportDetails, inOutState.mSurface.mQueueFamilyIndices, 
		extent, inOutState.mSurface.mSurfaceFormat, inOutState.mSurface.mDepthFormat, inOutState.mSurface.mMsaaCount, inOutState.mSurface.mMemoryProperties);
	
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



	state.mSurface = NPoVulkanSurfaceBehavior::init(inOutResources.mSurface, settings.mSurface, vulkanInstance, inOutResources.mpWindow);



	return state;
}

void NPoWindowBehavior::cleanup(SPoWindowResources &inOutResources, SPoWindowState &inOutState, SPoWindowSettings const &settings)
{
	NPoVulkanSurfaceBehavior::cleanup(inOutResources.mSurface, inOutState.mSurface, settings.mSurface);

	glfwDestroyWindow(inOutResources.mpWindow);
	inOutResources.mpWindow = nullptr;
}