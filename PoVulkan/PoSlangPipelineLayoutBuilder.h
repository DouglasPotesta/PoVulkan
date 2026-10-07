#pragma once

#include <slang/slang.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


namespace NPoPipelineLayoutBuilder
{
	inline void check_vk(VkResult result, const char *operation)
	{
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error(std::string(operation) + ": VkResult=" + std::to_string(result));
		}
	}

	inline uint32_t checked_count(size_t value)
	{
		if (value >= std::numeric_limits<uint32_t>::max())
		{
			throw std::runtime_error("Unobounded, unresolved, or oversized reflection count");
		}
		return static_cast<uint32_t>(value);
	}

	inline VkShaderStageFlags shader_stage(SlangStage stage)
	{
		switch (stage)
		{
		case SlangStage::SLANG_STAGE_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
		case SlangStage::SLANG_STAGE_HULL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		case SlangStage::SLANG_STAGE_DOMAIN: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		case SlangStage::SLANG_STAGE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
		case SlangStage::SLANG_STAGE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
		case SlangStage::SLANG_STAGE_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
		default: throw std::runtime_error("Only supports conventional graphics and compute stages");
		}
	}

	inline VkDescriptorType descriptor_type(slang::BindingType type)
	{
		switch (type)
		{
		case slang::BindingType::Sampler:	return VK_DESCRIPTOR_TYPE_SAMPLER;
		case slang::BindingType::CombinedTextureSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case slang::BindingType::Texture: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case slang::BindingType::MutableTexture:	return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case slang::BindingType::TypedBuffer:	return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		case slang::BindingType::RawBuffer:		
		case slang::BindingType::MutableRawBuffer:	  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case slang::BindingType::ConstantBuffer:	  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case slang::BindingType::InputRenderTarget:	  return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		default: throw std::runtime_error("Unsupported slang descriptor type: " + std::to_string(int(type)));
		}
	}

	struct SLayoutDescription
	{
		std::vector<std::vector<VkDescriptorSetLayoutBinding>> mSets;
		std::vector<VkPushConstantRange> mPushConstants;
	};

	struct SResourceValue
	{
		bool mAssigned = false;
		VkDescriptorImageInfo mImage = {};
		VkDescriptorBufferInfo mBuffer = {};
	};

	struct SResourceRange
	{
		uint32_t mSet = 0;
		uint32_t mBinding = 0;
		VkDescriptorType mType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		std::vector<SResourceValue> mElements;
	};

	struct SShaderObject
	{
		slang::TypeLayoutReflection *mpType = nullptr;
		std::vector<std::byte> mBytes;
		std::optional<SResourceRange> mUniformBuffer;
		bool mPushConstant = false;
		std::vector<std::optional<SResourceRange>> mRanges;
		std::vector<std::shared_ptr<SShaderObject>> mChildren;
		explicit SShaderObject(slang::TypeLayoutReflection *pLayout) : mpType(pLayout)
		{
			if (mpType == nullptr) throw std::runtime_error("Missing shader-object type");
			auto const count = checked_count(mpType->getBindingRangeCount());
			mRanges.resize(count);
			mChildren.resize(count);
		}
	};

	struct SProgramParameters
	{
		std::shared_ptr<SShaderObject> mGlobals;
		std::vector<std::shared_ptr<SShaderObject>> mEntryPoints;
	};

	namespace NPrivate
	{
		void remap_sets(std::shared_ptr<SShaderObject> const &pObject, std::vector<uint32_t> const &remap)
		{
			if (pObject->mUniformBuffer) pObject->mUniformBuffer->mSet = remap[pObject->mUniformBuffer->mSet];
			for (auto &range : pObject->mRanges) if (range) range->mSet = remap[range->mSet];
			for (auto &child : pObject->mChildren) if (child) remap_sets(child, remap);
		}

		size_t reserve_set(SLayoutDescription &inOutDescription)
		{
			inOutDescription.mSets.emplace_back();
			return inOutDescription.mSets.size() - 1;
		}

		void add_descriptor(SLayoutDescription &inOutDescription, size_t set, VkDescriptorType type, uint32_t count, VkShaderStageFlags stages)
		{
			if (count == 0) return;
			VkDescriptorSetLayoutBinding binding = { };
			binding.binding = checked_count(inOutDescription.mSets[set].size());
			binding.descriptorType = type;
			binding.descriptorCount = count;
			binding.stageFlags = stages;
			inOutDescription.mSets[set].push_back(binding);
		}

		void add_push_constants(SLayoutDescription &inOutDescription, slang::TypeLayoutReflection *pElement, VkShaderStageFlags stages)
		{
			auto const bytes = checked_count(pElement->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
			if (bytes == 0) return;
			if (bytes > std::numeric_limits<uint32_t>::max() - 3)
				throw std::runtime_error("Push-constant size overflow");
			uint32_t roundedSize = (bytes + 3u) & ~3u;
			// All supported push-constant blocks start at byte zero. One union range avoids repeating a stage bit in multiple Vulkan ranges.
			if (inOutDescription.mPushConstants.empty())
			{
				inOutDescription.mPushConstants.push_back({ stages, 0, roundedSize });
			}
			else
			{
				auto &range = inOutDescription.mPushConstants.front();
				range.stageFlags |= stages;
				range.size = std::max(range.size, roundedSize);
			}
		}
		void add_ranges(SLayoutDescription &inOutDescription, slang::TypeLayoutReflection *pType, size_t set, VkShaderStageFlags stages,
			std::shared_ptr<SShaderObject> const &pObject);
		std::shared_ptr<SShaderObject> add_block(SLayoutDescription &inOutDescription, slang::TypeLayoutReflection *pBlock, size_t set, VkShaderStageFlags stages, bool push)
		{
			auto pElement = pBlock->getElementTypeLayout();
			if (pElement == nullptr) throw std::runtime_error("Missing block element layout");
			auto pObject = std::make_shared<SShaderObject>(pElement);
			pObject->mBytes.resize(checked_count(pElement->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM)));
			pObject->mPushConstant = push;
			if (push)
			{
				add_push_constants(inOutDescription, pElement, stages);
			}
			else if (pElement->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM) != 0)
			{
				pObject->mUniformBuffer = SResourceRange{ checked_count(set), checked_count(inOutDescription.mSets[set].size()),
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, std::vector<SResourceValue>(1) };
				add_descriptor(inOutDescription, set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, stages);
			}
			add_ranges(inOutDescription, pElement, set, stages, pObject);
			return pObject;
		}


		void add_ranges(SLayoutDescription &inOutDescription, slang::TypeLayoutReflection *pType, size_t set, VkShaderStageFlags stages,
			std::shared_ptr<SShaderObject> const &pObject)
		{
			if (pType == nullptr) throw std::runtime_error("Missing type layout");
				

			for (SlangInt r = 0; r < pType->getBindingRangeCount(); ++r)
			{
				slang::BindingType kind = pType->getBindingRangeType(r);
				if (kind == slang::BindingType::VaryingInput || kind == slang::BindingType::VaryingOutput) continue;
				if (kind == slang::BindingType::ExistentialValue || kind == slang::BindingType::InlineUniformData)
					throw std::runtime_error("No support for specialized interface/inline-uniform layouts for now.");

				if (kind == slang::BindingType::ConstantBuffer ||
					kind == slang::BindingType::ParameterBlock ||
					kind == slang::BindingType::PushConstant)
				{
					if (pType->getBindingRangeBindingCount(r) != 1)
						throw std::runtime_error("No support for arrays of block sub-objects.");
					auto leaf = pType->getBindingRangeLeafTypeLayout(r);
					if (leaf == nullptr) throw std::runtime_error("Missing sub-object type layout");
					if (kind == slang::BindingType::ParameterBlock)
					{
						size_t const childSet = reserve_set(inOutDescription);
						pObject->mChildren[r] = add_block(inOutDescription, leaf, childSet, stages, false);

					}
					else
					{
						pObject->mChildren[r] = add_block(inOutDescription, leaf, set, stages, kind == slang::BindingType::PushConstant);
					}
					continue;
				}

				SlangInt count = pType->getBindingRangeDescriptorRangeCount(r);
				if (count == 0) continue;
				if (count != 1) throw std::runtime_error("Cursor backend requires one descriptor range per resource binding range");
				SlangInt const relativeSet = pType->getBindingRangeDescriptorSetIndex(r);
				SlangInt const first = pType->getBindingRangeFirstDescriptorRangeIndex(r);
				for (SlangInt j = 0; j < count; ++j)
				{
					SlangInt range = first + j;
					auto const leafKind = pType->getDescriptorSetDescriptorRangeType(relativeSet, range);
					auto const descriptors = pType->getDescriptorSetDescriptorRangeDescriptorCount(relativeSet, range);
					pObject->mRanges[r] = SResourceRange{ checked_count(set), checked_count(inOutDescription.mSets[set].size()),
						descriptor_type(leafKind), std::vector<SResourceValue>(checked_count(descriptors)) };
					add_descriptor(inOutDescription, set, descriptor_type(leafKind), checked_count(descriptors), stages);
				}
			}
		}

		SLayoutDescription build_layout_description_private(SProgramParameters &outParameters, slang::ProgramLayout *pProgram)
		{
			if (pProgram == nullptr) throw std::runtime_error("Null program layout");
			SLayoutDescription description = {};
			VkShaderStageFlags programStages = 0;
			for (SlangUInt i = 0; i < pProgram->getEntryPointCount(); ++i)
			{
				auto const stage = shader_stage(pProgram->getEntryPointByIndex(i)->getStage());
				if (programStages & stage)
					throw std::runtime_error("Reflect one entry point per pipeline stage");
				programStages |= stage;
			}
			if (programStages == 0) throw std::runtime_error("The linked program has no entry points");
			if ((programStages & VK_SHADER_STAGE_COMPUTE_BIT) && programStages != VK_SHADER_STAGE_COMPUTE_BIT)
				throw std::runtime_error("Compute and graphics entry points belong to separate pipelines");

			// reserve the implicit/derfault set before discovering child blocks.
			size_t const defaultSet = reserve_set(description);
			outParameters = {};
			outParameters.mGlobals = std::make_shared<SShaderObject>(pProgram->getGlobalParamsTypeLayout());
			add_ranges(description, pProgram->getGlobalParamsTypeLayout(), defaultSet, programStages, outParameters.mGlobals);
			for (SlangUInt i = 0; i < pProgram->getEntryPointCount(); ++i)
			{
				auto pEntryPoint= pProgram->getEntryPointByIndex(i);
				auto object = std::make_shared<SShaderObject>(pEntryPoint->getTypeLayout());
				add_ranges(description, pEntryPoint->getTypeLayout(), defaultSet, shader_stage(pEntryPoint->getStage()), object);
				outParameters.mEntryPoints.push_back(object);
			}

			auto &sets = description.mSets;
			std::vector<uint32_t> remap(sets.size());
			for (auto &object : outParameters.mEntryPoints) remap_sets(object, remap);
			sets.erase(std::remove_if(sets.begin(), sets.end(), [](auto const &set) {return set.empty(); }), sets.end());
			return description;
		}
	}
	SLayoutDescription build_layout_description(SProgramParameters &outParameters, slang::ProgramLayout *pProgram)
	{
		return NPrivate::build_layout_description_private(outParameters, pProgram);
	}
	SLayoutDescription build_layout_description(slang::ProgramLayout *pProgram)
	{
		SProgramParameters outParameters;
		return NPrivate::build_layout_description_private(outParameters, pProgram);
	}

	class CPipelineLayouts
	{
	public:
		VkDevice mDevice = VK_NULL_HANDLE;
		std::vector<VkDescriptorSetLayout> mSets;
		VkPipelineLayout mPipeline = VK_NULL_HANDLE;

		explicit CPipelineLayouts(VkDevice device) : mDevice(device) {}
		CPipelineLayouts(CPipelineLayouts &) = delete;
		CPipelineLayouts &operator=(CPipelineLayouts const &) = delete;
		CPipelineLayouts(CPipelineLayouts &&other) noexcept
			: mDevice(other.mDevice), mSets(std::move(other.mSets)),
			mPipeline(std::exchange(other.mPipeline, VK_NULL_HANDLE))
		{
			other.mDevice = VK_NULL_HANDLE;
		}
		~CPipelineLayouts()
		{
			if (mDevice == VK_NULL_HANDLE) return;
			if (mPipeline != VK_NULL_HANDLE) vkDestroyPipelineLayout(mDevice, mPipeline, nullptr);
			for (auto set : mSets)
				if (set != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(mDevice, set, nullptr);
		}
		
		static inline CPipelineLayouts CreatePipelineLayouts(VkDevice device, VkPhysicalDeviceLimits &limits, SLayoutDescription const &description)
		{
			if (description.mSets.size() > limits.maxBoundDescriptorSets)
				throw std::runtime_error("Layout exceeds max bound descriptor sets");
			for (auto const &range : description.mPushConstants)
			{
				if (uint64_t(range.offset) + range.size > limits.maxPushConstantsSize)
					throw std::runtime_error("Layout exceeds max push constants size");
			}
			CPipelineLayouts result(device);
			// Allocate storage before creating handes, so allocation failure cannot leak them.
			result.mSets.resize(description.mSets.size(), VK_NULL_HANDLE);
			for (size_t i = 0; i < description.mSets.size(); ++i)
			{
				VkDescriptorSetLayoutCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				info.bindingCount = checked_count(description.mSets[i].size());
				info.pBindings = description.mSets[i].data();
				check_vk(vkCreateDescriptorSetLayout(device, &info, nullptr, &result.mSets[i]), "vkCreateDescriptorSetLayout");
			}
			VkPipelineLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			info.setLayoutCount = checked_count(result.mSets.size());
			info.pSetLayouts = result.mSets.data();
			info.pushConstantRangeCount = checked_count(description.mPushConstants.size());
			info.pPushConstantRanges = description.mPushConstants.data();
			check_vk(vkCreatePipelineLayout(device, &info, nullptr, &result.mPipeline), "vkCreatePipelineLayout");
			return result;
		}
	};

}