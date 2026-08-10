#pragma once

#include "PoGlm.h"

struct SPoVulkanUniformSettings
{

};

struct SPoVulkanUniformState
{
	glm::mat4 model = glm::mat4(0);
	glm::mat4 view = glm::mat4(0);
	glm::mat4 projection = glm::mat4(0);
};

struct SPoVulkanUniformResources
{

};


namespace NPoVulkanUniformBehavior
{
	SPoVulkanUniformState init(SPoVulkanUniformResources &outResources, SPoVulkanUniformSettings const &settings);
	void cleanup(SPoVulkanUniformResources &inOutResources, SPoVulkanUniformState &inOutState, SPoVulkanUniformSettings const &settings);
}