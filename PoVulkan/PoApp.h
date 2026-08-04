#pragma once

#include "PoWindow.h"
#include "PoVulkan.h"
#include "PoVulkanSurface.h"

struct SPoAppSettings
{
	SPoWindowSettings mWindow;
	SPoVulkanSettings mVulkan;
};

struct SPoAppState
{
	SPoWindowState mWindow;
	SPoVulkanState mVulkan;
};

struct SPoAppResources
{
	SPoWindowResources mWindow;
	SPoVulkanResources mVulkan;
};

namespace NPoAppBehavior
{
	void run();
}