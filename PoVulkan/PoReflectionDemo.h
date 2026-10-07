#pragma once

#include "PoBindlessResources.h"
#include "PoVulkanDevice.h"
#include <iostream>
#include <functional>

namespace NPoReflectionDemo
{
	using namespace NPoBindlessResources;
	using namespace NPoPipelineLayoutBuilder;
	using namespace NPoShaderCursor;
	using namespace NPoVulkanDeviceBehavior;

	void run_demo(SPoVulkanDeviceResources deviceResources, SPoVulkanDeviceState deviceState, SPoVulkanDeviceSettings deviceSettings,
		SLayoutDescription const &description, CPipelineLayouts const &layouts,
		SProgramParameters const &parameters, VkShaderModule const &shaderModule)
	{
		VkDevice &device = deviceResources.mLogicalDevice;
		VkPhysicalDeviceMemoryProperties memoryProperties = deviceState.mMemoryProperties;
		struct SScopedResources
		{
			std::vector<VkBuffer> mBuffers;
			std::vector<VkDeviceMemory> mBufferMemories;
			std::vector<void *> mBufferMaps;
			std::vector<SPoTextureResources *> mTextureResources;
			std::vector<SPoTextureState *> mTextureStates;
			VkSampler mSampler;
			VkDescriptorPool mPool;
			std::vector<VkDescriptorSet> mSets;
			VkDevice mDevice = VK_NULL_HANDLE;
			VkPipeline mPipeline;
			void AddBufferGroup(VkBuffer buf, VkDeviceMemory mem, void *map)
			{
				mBuffers.push_back(buf);
				mBufferMemories.push_back(mem);
				mBufferMaps.push_back(map);
			}
			void AddPoTexture(SPoTextureResources *pTextureResources, SPoTextureState *pTextureState)
			{
				mTextureResources.push_back(pTextureResources);
				mTextureStates.push_back(pTextureState);
			}
			~SScopedResources()
			{
				vkDestroyPipeline(mDevice, mPipeline, nullptr);
				for (int i = 0; i < mBuffers.size(); ++i)
				{
					vkUnmapMemory(mDevice, mBufferMemories[i]);
					vkFreeMemory(mDevice, mBufferMemories[i], nullptr);
					vkDestroyBuffer(mDevice, mBuffers[i], nullptr);
				}
				vkDestroySampler(mDevice, mSampler, nullptr);
				for (int i = 0; i < mTextureResources.size(); ++i)
				{
					NPoTextureBehavior::cleanup(*mTextureResources[i], *mTextureStates[i], {});
				}
				vkDestroyDescriptorPool(mDevice, mPool, nullptr);
			}
		};
		SScopedResources trackedResources = {};
		trackedResources.mDevice = device;

		VkBuffer resultBuffer;
		VkDeviceMemory resultMemory;
		VkDeviceSize const resultSize = sizeof(float) * 4;
		create_buffer(resultBuffer, resultMemory, device, resultSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memoryProperties);
		void *resultMapped;
		vkMapMemory(device, resultMemory, 0, resultSize, 0, &resultMapped);
		std::memset(resultMapped, 0, resultSize);
		trackedResources.AddBufferGroup(resultBuffer, resultMemory, resultMapped);
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxLod = 0;
		VkSampler defaultSampler;
		vkCreateSampler(device, &samplerInfo, nullptr, &defaultSampler);
		trackedResources.mSampler = defaultSampler;
		SSceneData scene;
		std::array <SPoTextureState, SMaterialData{}.mLayers.size() > textureStates;
		std::array <SPoTextureResources, SMaterialData{}.mLayers.size() > textureResources;
		SPoTextureSettings textureSettings;
		VkCommandPool &commandPool = deviceResources.mCommandPool;
		VkQueue graphicsQueue = deviceState.mGraphicsQueue;
		for (int i = 0; i < textureStates.size(); ++i)
		{
			textureStates[i] = NPoTextureBehavior::init(textureResources[i], textureSettings,
				device, deviceState.mPhysicalDevice, commandPool, graphicsQueue,
				memoryProperties);
			scene.mMaterial.mLayers[i].mView = textureResources[i].mImageView;
			trackedResources.AddPoTexture(&(textureResources[i]), &(textureStates[i]));
		}
		scene.mMaterial.mSampler = SSampler{ .mHandle = defaultSampler };
		WriteParameters(parameters, scene, { resultBuffer, 0, resultSize }, 1);
		std::unordered_map<VkDescriptorType, uint32_t> counts;
		for (auto const &set : description.mSets)
		{
			for (auto const &binding : set)
			{
				counts[binding.descriptorType] += binding.descriptorCount;
			}
		}
		if (description.mSets.empty())
		{
			return;
		}
		std::vector<VkDescriptorPoolSize> poolSizes;
		for (auto const &item : counts)
		{
			poolSizes.push_back({ item.first, item.second });
		}
		VkDescriptorPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.maxSets = checked_count(description.mSets.size());
		info.poolSizeCount = checked_count(poolSizes.size());
		info.pPoolSizes = poolSizes.data();
		VkDescriptorPool pool;
		vkCreateDescriptorPool(device, &info, nullptr, &pool);
		trackedResources.mPool = pool;
		std::vector<VkDescriptorSet> sets;
		sets.resize(description.mSets.size());
		VkDescriptorSetAllocateInfo descriptorAllocateInfo = {};
		descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorAllocateInfo.descriptorPool = pool;
		descriptorAllocateInfo.pSetLayouts = layouts.mSets.data();
		descriptorAllocateInfo.descriptorSetCount = static_cast<uint32_t>(layouts.mSets.size());
		vkAllocateDescriptorSets(device, &descriptorAllocateInfo, sets.data());
		trackedResources.mSets = sets;

		auto write_range = [](std::vector<VkDescriptorSet> &inOutSets, VkDevice device, SResourceRange const &range)
			{
				for (uint32_t i = 0; i < range.mElements.size(); ++i)
				{
					auto const &value = range.mElements[i];
					if (!value.mAssigned) throw std::runtime_error("Unassigned descriptor element");
					VkWriteDescriptorSet write = {};
					write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write.dstSet = inOutSets[range.mSet];
					write.dstBinding = range.mBinding;
					write.dstArrayElement = i;
					write.descriptorCount = 1;
					write.descriptorType = range.mType;
					switch (range.mType)
					{
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
						if (value.mBuffer.buffer == VK_NULL_HANDLE) throw std::runtime_error("Null descriptor buffer handle");
						write.pBufferInfo = &value.mBuffer;
						break;
					case VK_DESCRIPTOR_TYPE_SAMPLER:
						if (value.mImage.sampler == VK_NULL_HANDLE) throw std::runtime_error("Null descriptor sampler handle");
						write.pImageInfo = &value.mImage;
						break;
					case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
						if (value.mImage.imageView == VK_NULL_HANDLE) throw std::runtime_error("Null descriptor image handle");
						write.pImageInfo = &value.mImage;
						break;
					default: throw std::runtime_error("Descriptor writer usnupported for this type");
					}
					vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
				}
			};
		std::vector<std::byte> pushBytes;
		std::function<void(std::vector<std::byte> &, 
			std::vector<VkDescriptorSet> &, 
			VkDevice , 
			SScopedResources &, 
			std::shared_ptr<SShaderObject> const &, 
			VkPhysicalDeviceMemoryProperties const &)> upload_object = 
			[&write_range, &upload_object](std::vector<std::byte> &outPushBytes, std::vector<VkDescriptorSet> &inOutSets, VkDevice device, SScopedResources &trackedResources, std::shared_ptr<SShaderObject> const &pObject, VkPhysicalDeviceMemoryProperties const &memoryProperties)
			{
				if (pObject->mUniformBuffer)
				{
					VkBuffer buffer;
					VkDeviceMemory memory;
					void *pMapped;
					create_buffer(buffer, memory, device, pObject->mBytes.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memoryProperties);
					vkMapMemory(device, memory, 0, pObject->mBytes.size(), 0, &pMapped);
					trackedResources.AddBufferGroup(buffer, memory, pMapped);
					std::memcpy(pMapped, pObject->mBytes.data(), pObject->mBytes.size());
					auto range = *pObject->mUniformBuffer;
					range.mElements[0].mBuffer = { buffer, 0, pObject->mBytes.size() };
					range.mElements[0].mAssigned = true;
					write_range(inOutSets, device, range);
				}
				if (pObject->mPushConstant && !pObject->mBytes.empty())
				{
					if (!outPushBytes.empty()) throw std::runtime_error("only on push data object supported.");
					outPushBytes = pObject->mBytes;
				}
				for (auto const &range : pObject->mRanges)
				{
					if (range)
					{
						write_range(inOutSets, device, *range);
					}
				}
				for (auto const &child : pObject->mChildren)
				{
					if (child)
					{
						upload_object(outPushBytes, inOutSets, device, trackedResources, child, memoryProperties);
					}
				}
			};
		upload_object(pushBytes, sets, device, trackedResources, parameters.mGlobals, memoryProperties);
		for (auto const &entry : parameters.mEntryPoints)
		{
			upload_object(pushBytes, sets, device, trackedResources, entry, memoryProperties);
		}

		VkComputePipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineInfo.stage.module = shaderModule;
		pipelineInfo.layout = layouts.mPipeline;
		pipelineInfo.stage.pName = "main";
		VkPipeline computePipeline = VK_NULL_HANDLE;
		vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);
		trackedResources.mPipeline = computePipeline;

		VkCommandBufferAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandBufferCount = 1;
		allocateInfo.commandPool = commandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);
		NPoVulkanDeviceBehavior::begin_command_buffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		for (int i = 0; i < textureStates.size(); ++i)
		{
			transition_image_layout_command(commandBuffer, textureResources[i].mImage, textureSettings.mImageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
			VkClearColorValue color = { {0.5f, 0.25f, 0.125f, 1.0f} };
			VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1};
			vkCmdClearColorImage(commandBuffer, textureResources[i].mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &subresourceRange);
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = textureResources[i].mImage;
			barrier.subresourceRange = subresourceRange;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		}
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		if (!sets.empty())
		{
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, layouts.mPipeline,
				0, checked_count(sets.size()), sets.data(), 0, nullptr);
		}
		if (!description.mPushConstants.empty())
		{
			if (description.mPushConstants.size() != 1 || pushBytes.empty())
				throw std::runtime_error("Missing or unsupported push-constant storage");
			auto const &range = description.mPushConstants.front();
			if (range.offset != 0 || range.stageFlags != VK_SHADER_STAGE_COMPUTE_BIT || pushBytes.size() > range.size)
				throw std::runtime_error("Unsupported push range in compute backend");
			pushBytes.resize(range.size, std::byte{ 0 });
			vkCmdPushConstants(commandBuffer, layouts.mPipeline, range.stageFlags, range.offset, range.size, pushBytes.data());
		}

		vkCmdDispatch(commandBuffer, 1, 1, 1);
		VkMemoryBarrier readback = {};
		readback.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		readback.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		readback.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &readback, 0, nullptr, 0, nullptr);
		vkEndCommandBuffer(commandBuffer);
		VkSubmitInfo submit = { };
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &commandBuffer;
		vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		std::array<float, 4> actual = {};
		std::memcpy(actual.data(), resultMapped, sizeof(actual));
		std::cout << "gpu output : " << actual[0] << ", " << actual[1] << ", " << actual[2] << ", " << actual[3];
		
	}

}
