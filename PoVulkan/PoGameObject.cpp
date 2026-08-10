#include "PoGameObject.h"
#include "PoVulkanDevice.h"
#include "PoVulkanUniform.h"

#include <array>
#include <stdexcept>
#include <chrono>

namespace NPoGameObjectPrivate
{

    void create_descriptor_sets(std::vector<VkDescriptorSet> &descriptorSets, std::vector<VkBuffer> &uniformBuffers, VkImageView textureImageView, VkSampler textureSampler, VkDevice device, std::vector<VkDescriptorSetLayout> &layouts, VkDescriptorPool descriptorPool)
    {
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
		allocInfo.pSetLayouts = layouts.data();
		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		for (size_t i = 0; i < descriptorSets.size(); ++i)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(SPoVulkanUniformState);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureImageView;
			imageInfo.sampler = textureSampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo;
			descriptorWrites[0].pImageInfo = nullptr;
			descriptorWrites[0].pTexelBufferView = nullptr;


			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
    }

}

SPoGameObjectState NPoGameObjectBehavior::init(SPoGameObjectResources &outResources, SPoGameObjectSettings const &settings,
    VkDevice device, VkImageView textureImageView, VkSampler textureSampler, std::vector<VkDescriptorSetLayout> &layouts, VkDescriptorPool descriptorPool,
    VkPhysicalDeviceMemoryProperties const &memoryProperties, int const numFrames)
{
    using namespace NPoGameObjectPrivate;
    SPoGameObjectState state = {};
	state.mDevice = device;
	VkDeviceSize bufferSize = sizeof(SPoVulkanUniformState);
    outResources.mUniformBuffers.reserve(numFrames);
    outResources.mUniformBuffersMemory.reserve(numFrames);
    outResources.mUniformBuffersMapped.reserve(numFrames);
    outResources.mDescriptorSets.reserve(numFrames);

    outResources.mUniformBuffers.resize(numFrames);
    outResources.mUniformBuffersMemory.resize(numFrames);
    outResources.mUniformBuffersMapped.resize(numFrames);
    outResources.mDescriptorSets.resize(numFrames);
    for (int i = 0; i < numFrames; ++i)
    {
        NPoVulkanDeviceBehavior::create_buffer (outResources.mUniformBuffers[i], outResources.mUniformBuffersMemory[i],
            device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            memoryProperties);
        vkMapMemory(device, outResources.mUniformBuffersMemory[i], 0, bufferSize, 0, &(outResources.mUniformBuffersMapped[i]));
    }
	create_descriptor_sets(outResources.mDescriptorSets, outResources.mUniformBuffers, textureImageView, textureSampler, device, layouts, descriptorPool);
    return state;
}

void NPoGameObjectBehavior::update_uniform_buffers(SPoGameObjectResources &resources, SPoGameObjectState const &state, SPoGameObjectSettings const &settings, int const frameIndex, glm::mat4 const &view, glm::mat4 const &projection)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	int loopIndex = (reinterpret_cast<uint64_t>(&state) / sizeof(SPoGameObjectState)) % (20);
	float cycle = glm::two_pi<float>() * (loopIndex / 20.0f) + time;
	float y = glm::sin(cycle);
	float x = glm::cos(cycle);
	SPoTransform transform = state.mTransform;
	// transform.mPosition.x = x;
	transform.mPosition.y = y;

	SPoVulkanUniformState ubo{};
	ubo.model = transform.ToMatrix() * glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	//ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	ubo.view = view;

	ubo.projection = projection;

	memcpy(resources.mUniformBuffersMapped[frameIndex], &ubo, sizeof(ubo));
}

void NPoGameObjectBehavior::cleanup(SPoGameObjectResources &inOutResources, SPoGameObjectState &inOutState, SPoGameObjectSettings const &settings)
{
    for (int i = 0; i < inOutResources.mUniformBuffers.size(); ++i)
    {
        vkDestroyBuffer(inOutState.mDevice, inOutResources.mUniformBuffers[i], nullptr);
        vkFreeMemory(inOutState.mDevice, inOutResources.mUniformBuffersMemory[i], nullptr);
    }
    inOutResources = {};
    inOutState = {};
}
