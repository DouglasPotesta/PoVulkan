#pragma once

#include "PoGlm.h"
#include "PoGlfw.h"
#include "PoTransform.h"

/// <summary>
/// This is temporary. Will remove this in favor of something like a draw call structure
/// </summary>

struct SPoGameObjectSettings
{
};

struct SPoGameObjectState
{
	SPoTransform mTransform;
	glm::vec3 mVelocity = { 0.0f, 0.0f, 0.0f };
	VkDevice mDevice = VK_NULL_HANDLE;
	// this is used to help identify which group it renders with
	VkPipeline mPipeline = VK_NULL_HANDLE;
	
};

struct SPoGameObjectResources
{
	std::vector<VkBuffer> mUniformBuffers;
	std::vector<VkDeviceMemory> mUniformBuffersMemory;
	std::vector<void *> mUniformBuffersMapped;

	std::vector<VkDescriptorSet> mDescriptorSets;
};

namespace NPoGameObjectBehavior
{
	size_t const gk_max_game_objects = 10;
	SPoGameObjectState init(SPoGameObjectResources &outResources, SPoGameObjectSettings const &settings,
		VkDevice device, VkImageView textureImageView, VkSampler textureSampler, std::vector<VkDescriptorSetLayout> &layouts, VkDescriptorPool descriptorPool,
		VkPhysicalDeviceMemoryProperties const &memoryProperties, int const numFrames);

	void update_uniform_buffers(SPoGameObjectResources &resources, SPoGameObjectState const &state, SPoGameObjectSettings const &settings,
		int const frameIndex, glm::mat4 const &view, glm::mat4 const &projection);

	void cleanup(SPoGameObjectResources &inOutResources, SPoGameObjectState &inOutState, SPoGameObjectSettings const &settings);

}