
#include "PoWindow.h"


static void framebufferResizeCallback(GLFWwindow *pWindow, int width, int height)
{
	if (void *pPointer = glfwGetWindowUserPointer(pWindow))
	{
		SPoWindowResizeCommand &command = *reinterpret_cast<SPoWindowResizeCommand *>(pPointer);
		command.mpState->mDevice.mFramebufferResized = true;
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