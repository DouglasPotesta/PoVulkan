#pragma once
#include "PoGlfw.h"
#include "StaticVector.h"

#include <optional>
#include <cstring>
#include <vector>



int const gkMaxNumberOfDevices = 8;



struct SPoVulkanDeviceSettings
{

};

struct SPoVulkanDeviceState
{
	VkPhysicalDevice mPhysicalDevice;
};

struct SPoVulkanDeviceResources
{
	VkDevice mDevice;
};

namespace NPoVulkanDeviceBehavior
{
	SPoVulkanDeviceState init(SPoVulkanDeviceResources &inOutResource, SPoVulkanDeviceSettings const &settings);

}