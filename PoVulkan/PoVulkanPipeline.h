#pragma once
#include "PoGlfw.h"


struct SPoSlangState;
struct SPoSlangResources;

struct SPoVulkanPipelineSettings
{

};

struct SPoVulkanPipelineState
{
	VkDevice mDevice = VK_NULL_HANDLE;

};

struct SPoVulkanPipelineResources
{
	VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
	VkPipeline mPipeline = VK_NULL_HANDLE;
	
};


namespace NPoVulkanPipelineBehavior
{
	SPoVulkanPipelineState init(SPoVulkanPipelineResources &outResources, SPoVulkanPipelineSettings const &settings, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkExtent2D const &extent, VkSampleCountFlagBits const msaaSamples, SPoSlangState const &slangState, SPoSlangResources &slangResources, VkFormat const colorFormat, VkFormat const depthFormat);
	void cleanup(SPoVulkanPipelineResources &inOutResources, SPoVulkanPipelineState &inOutState, SPoVulkanPipelineSettings const &settings);
}