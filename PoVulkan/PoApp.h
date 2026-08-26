#pragma once

#include "PoWindow.h"
#include "PoVulkan.h"
#include "PoVulkanDevice.h"

struct SPoAppSettings
{
	SPoWindowSettings mWindow;
	SPoVulkanSettings mVulkan;
	SPoVulkanDeviceSettings mDevice;
};

struct SPoAppState
{
	std::vector<SPoWindowState> mWindows;
	SPoVulkanState mVulkan;
	SPoVulkanDeviceState mDevice;
};

struct SPoAppResources
{
	std::vector<SPoWindowResources> mWindows;
	SPoVulkanResources mVulkan = {};
	SPoVulkanDeviceResources mDevice = {};
};

namespace NPoAppBehavior
{
	void run();
}