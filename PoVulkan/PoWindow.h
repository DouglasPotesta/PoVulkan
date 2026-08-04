#pragma once

#include "PoGlfw.h"
#include "PoVulkanSurface.h"

#include <string>
#include <memory>
#include <memory>


struct SPoWindowResizeCommand
{
	struct SPoWindowSettings *mpSettings = nullptr;
	struct SPoWindowResources *mpResources = nullptr;
	struct SPoWindowState *mpState = nullptr;
};


struct SPoWindowSettings
{
	int mWidth = 800;
	int mHeight = 600;
	std::string mName = "PoVulkan";
	SPoVulkanSurfaceSettings mSurface;

};


struct SPoWindowResources
{
	GLFWwindow *mpWindow = nullptr;
	SPoVulkanSurfaceResources mSurface;
	SPoWindowResizeCommand mResizeCommand;
};

struct SPoWindowState
{
	VkInstance mVulkanInstance = VK_NULL_HANDLE;
	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	SPoVulkanSurfaceState mSurface = {};
	int mResizeCommandId = 0;
};

namespace NPoWindowBehavior
{
	SPoWindowState init(SPoWindowResources &inOutResources, SPoWindowSettings const &settings, VkInstance vulkanInstance);
	void cleanup(SPoWindowResources &inOutResources, SPoWindowState &inOutState, SPoWindowSettings const &settings);
	inline bool should_close(SPoWindowResources const &resources)
	{
		return glfwWindowShouldClose(resources.mpWindow);
	}

}