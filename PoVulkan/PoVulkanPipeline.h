#pragma once
#include "PoGlfw.h"
#include <string>

struct SPoSlangState;
struct SPoSlangResources;

struct SPoVulkanPipelineSettings
{
	static int const skMaxPipelines = 3;
};

struct SPoVulkanPipelineState
{
	VkDevice mDevice = VK_NULL_HANDLE;
	std::string mShaderPath;
	int mCurrentGeneration = -1;

};

struct SPoVulkanPipelineResources
{
	VkPipelineLayout mPipelineLayout[SPoVulkanPipelineSettings::skMaxPipelines] = { VK_NULL_HANDLE };
	VkPipeline mPipeline[SPoVulkanPipelineSettings::skMaxPipelines] = { VK_NULL_HANDLE };
	 
};
 

namespace NPoVulkanPipelineBehavior
{
	void get_current_pipeline(VkPipelineLayout &outPiplineLayout, VkPipeline &outGraphicsPipeline, SPoVulkanPipelineResources &resources, SPoVulkanPipelineState const &state);
	 
	void refresh_pipeline(SPoVulkanPipelineResources &resources, SPoVulkanPipelineState &state, SPoVulkanPipelineSettings const &settings,
		VkPipelineCache cache, VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
		SPoSlangState &slangState, SPoSlangResources &slangResources,
		VkSampleCountFlagBits const msaaSamples, VkFormat const colorFormat, VkFormat const depthFormat);

	SPoVulkanPipelineState init(SPoVulkanPipelineResources &outResources, SPoVulkanPipelineSettings const &settings,
		VkPipelineCache cache, VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
		SPoSlangState &slangState, SPoSlangResources &slangResources, char const *pShaderPath,
		VkSampleCountFlagBits const msaaSamples, VkFormat const colorFormat, VkFormat const depthFormat);
	void cleanup(SPoVulkanPipelineResources &inOutResources, SPoVulkanPipelineState &inOutState, SPoVulkanPipelineSettings const &settings);
}