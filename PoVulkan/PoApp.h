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
	SPoWindowState mWindow;
	SPoVulkanState mVulkan;
	SPoVulkanDeviceState mDevice;
};

struct SPoAppResources
{
	SPoWindowResources mWindow;
	SPoVulkanResources mVulkan;
	SPoVulkanDeviceResources mDevice;
};

namespace NPoAppBehavior
{
	void run();
}