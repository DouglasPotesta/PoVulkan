#include "PoVulkanUniform.h"

SPoVulkanUniformState NPoVulkanUniformBehavior::init(SPoVulkanUniformResources &outResources, SPoVulkanUniformSettings const &settings)
{
    SPoVulkanUniformState state;
    return state;
}

void NPoVulkanUniformBehavior::cleanup(SPoVulkanUniformResources &inOutResources, SPoVulkanUniformState &inOutState, SPoVulkanUniformSettings const &settings)
{
    inOutResources = {};
    inOutState = {};
}
