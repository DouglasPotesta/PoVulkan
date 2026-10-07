#include "PoVulkanPipeline.h"
#include "PoSlang.h"
#include "PoMesh.h"
#include "PoVulkanDevice.h"

namespace NPoVulkanPipelineStatePrivate
{
    void increment_and_generate(SPoVulkanPipelineResources &resources, SPoVulkanPipelineState &state, SPoVulkanPipelineSettings const &settings,
        VkPipelineCache cache, VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
        SPoSlangState &slangState, SPoSlangResources &slangResources,
        VkSampleCountFlagBits const msaaSamples, VkFormat const colorFormat, VkFormat const depthFormat)
    {
        ++(state.mCurrentGeneration);
        int const generationIndex = state.mCurrentGeneration % SPoVulkanPipelineSettings::skMaxPipelines;
        VkPipeline &graphicsPipeline = resources.mPipeline[generationIndex];
        VkPipelineLayout &pipelineLayout = resources.mPipelineLayout[generationIndex];
        if (state.mCurrentGeneration >= SPoVulkanPipelineSettings::skMaxPipelines)
        {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        }
        NPoVulkanDeviceBehavior::create_dynamic_graphics_pipeline(pipelineLayout, graphicsPipeline,
            cache, device, descriptorSetLayout,
            slangState, slangResources, state.mShaderPath.c_str(),
            msaaSamples, colorFormat, depthFormat);
    }
}


SPoVulkanPipelineState NPoVulkanPipelineBehavior::init(SPoVulkanPipelineResources &outResources, SPoVulkanPipelineSettings const &settings, 
	VkPipelineCache cache, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, 
	SPoSlangState &slangState, SPoSlangResources &slangResources, char const *pShaderPath,
	VkSampleCountFlagBits const msaaSamples, VkFormat const colorFormat, VkFormat const depthFormat)
{
    SPoVulkanPipelineState state;
    state.mDevice = device;
    state.mShaderPath = pShaderPath;

    NPoVulkanPipelineStatePrivate::increment_and_generate(outResources, state, settings,
        cache, device, descriptorSetLayout,
        slangState, slangResources,
        msaaSamples, colorFormat, depthFormat);

    return state;
}



void NPoVulkanPipelineBehavior::refresh_pipeline(SPoVulkanPipelineResources &resources, SPoVulkanPipelineState &state, SPoVulkanPipelineSettings const &settings,
    VkPipelineCache cache, VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
    SPoSlangState &slangState, SPoSlangResources &slangResources,
    VkSampleCountFlagBits const msaaSamples, VkFormat const colorFormat, VkFormat const depthFormat)
{
    {
        NPoVulkanPipelineStatePrivate::increment_and_generate(resources, state, settings,
            cache, device, descriptorSetLayout,
            slangState, slangResources,
            msaaSamples, colorFormat, depthFormat);
    }
}

void NPoVulkanPipelineBehavior::get_current_pipeline(VkPipelineLayout &outPiplineLayout, VkPipeline &outGraphicsPipeline, SPoVulkanPipelineResources &resources, SPoVulkanPipelineState const &state)
{
    if (state.mCurrentGeneration < 0)
    {
        throw std::runtime_error("failed to get current pipeline due to generation being negative!");
    }
    int const index = state.mCurrentGeneration % SPoVulkanPipelineSettings::skMaxPipelines;
    outPiplineLayout = resources.mPipelineLayout[index];
    outGraphicsPipeline = resources.mPipeline[index];
}

void NPoVulkanPipelineBehavior::cleanup(SPoVulkanPipelineResources &inOutResources, SPoVulkanPipelineState &inOutState, SPoVulkanPipelineSettings const &settings)
{
    int num = inOutState.mCurrentGeneration >= SPoVulkanPipelineSettings::skMaxPipelines ? SPoVulkanPipelineSettings::skMaxPipelines : inOutState.mCurrentGeneration + 1;
    for (int index = 0; index < num; ++index)
    {
        vkDestroyPipeline(inOutState.mDevice, inOutResources.mPipeline[index], nullptr);
        vkDestroyPipelineLayout(inOutState.mDevice, inOutResources.mPipelineLayout[index], nullptr);
    }
    inOutResources = {};
    inOutState = {};
}
