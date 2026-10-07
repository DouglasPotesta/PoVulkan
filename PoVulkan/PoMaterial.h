#pragma once

#include "PoGlfw.h"

#include <string>
#include <array>
#include <vector>

struct SPoShaderLayout
{
	VkShaderStageFlagBits mStage;
	std::string mShaderName;
	std::string mShaderEntryName; // the slang entry name not the main function name
	bool operator==(SPoShaderLayout const &) const = default;
	bool operator!=(SPoShaderLayout const &) const = default;
};

// TODO : use slang reflection to generate these
struct SPoDescriptorSetLayoutBinding
{
	uint32_t mBinding;
	VkDescriptorType mDescriptorType;
	uint32_t mDescriptorCount;
	VkShaderStageFlags mStageFlags;
	// ignoring immutable samplers as they are not needed right now
	bool operator==(SPoDescriptorSetLayoutBinding const &) const = default;
	bool operator!=(SPoDescriptorSetLayoutBinding const &) const = default;
};

// TODO : use slang reflection to generate these
struct SPoVertexInputBindingDescription
{
	uint32_t mBinding;
	uint32_t mStride;
	VkVertexInputRate mInputRate;
	bool operator==(SPoVertexInputBindingDescription const &) const = default;
	bool operator!=(SPoVertexInputBindingDescription const &) const = default;
};

// TODO : use slang reflection to generate these
struct SPoVertextInputAttributeDescription
{
	uint32_t mLocation;
	uint32_t mBinding;
	uint32_t mFormat;
	uint32_t mOffset;
	bool operator==(SPoVertextInputAttributeDescription const &) const = default;
	bool operator!=(SPoVertextInputAttributeDescription const &) const = default;
};

struct SPoPipelineRasterizationStateCreateInfo
{
	VkPipelineRasterizationStateCreateFlags mFlags = 0;
	bool mDepthClampEnable = false;
	bool mRasterizerDiscardEnable = false;
	VkPolygonMode mPolygonMode = VK_POLYGON_MODE_FILL;
	VkCullModeFlags mCullMode = VK_CULL_MODE_BACK_BIT;
	VkFrontFace mFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	bool mDepthBiasEnable = false;
	float mDepthBiasConstantFactor = 0.0f;
	float mDepthBiasClamp = 0.0f;
	float mDepthBiasSlopeFactor = 0.0f;
	float mLineWidth = 1.0f;
	bool operator==(SPoPipelineRasterizationStateCreateInfo const &) const = default;
	bool operator!=(SPoPipelineRasterizationStateCreateInfo const &) const = default;
};

struct SPoPipelineMultisampleStateCreateInfo
{
	VkPipelineMultisampleStateCreateFlags mFlags = 0;
	// sample count in a pipeline will typically do nothing outside of the rare case where sommeone uses attachmentless rendering
	// VkSampleCountFlagBits mRasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	bool mSampleShadingEnable = true;
	float mMinSampleShading = 0.2f;
	// no idea what a psample mask does so we'll ignore it for now
	// const VkSampleMask *pSampleMask;
	bool mAlphaToCoverageEnable = false;
	bool mAlphaToOneEnable = false;
	bool operator==(SPoPipelineMultisampleStateCreateInfo const &) const = default;
	bool operator != (SPoPipelineMultisampleStateCreateInfo const &) const = default;
};

struct SPoStencilOpState
{
	VkStencilOp mFailOp;
	VkStencilOp mPassOp;
	VkStencilOp mDepthFailOp;
	VkCompareOp mCompareOp;
	uint32_t mCompareMask;
	uint32_t mWriteMask;
	uint32_t mReference;
	bool operator==(SPoStencilOpState const &) const = default;
	bool operator!=(SPoStencilOpState const &) const = default;
};

struct SPoPipelineDepthStencilStateCreateInfo
{
	VkPipelineDepthStencilStateCreateFlags mFlags = 0;
	bool mDepthTestEnable = true;
	bool mDepthWriteEnable = true;
	VkCompareOp mDepthCompareOp = VK_COMPARE_OP_LESS;
	bool mDepthBoundsTestEnable = false;
	bool mStencilTestEnable = true;
	SPoStencilOpState mFront = {};
	SPoStencilOpState mBack = {};
	float mMinDepthBounds = 0.0f;
	float mMaxDepthBounds = 1.0f;
	bool operator==(SPoPipelineDepthStencilStateCreateInfo const &) const = default;
	bool operator!=(SPoPipelineDepthStencilStateCreateInfo const &) const = default;
};

struct SPoPipelineColorBlendAttachmentState
{
	bool mBlendEnable = false;
	VkBlendFactor mSrcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	VkBlendFactor mDstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	VkBlendOp mColorBlendOp = VK_BLEND_OP_ADD;
	VkBlendFactor mSrcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	VkBlendFactor mDstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	VkBlendOp mAlphaBlendOp = VK_BLEND_OP_ADD;
	VkColorComponentFlags mColorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	bool operator==(SPoPipelineColorBlendAttachmentState const &) const = default;
	bool operator!=(SPoPipelineColorBlendAttachmentState const &) const = default;
};

// a lot of these values could actually be derived from the shader itself using reflection from slang.
// annoyingly these just have to be kept in sync for now just by simply knowing what does what on both ends.
// TODO : Fix this
struct SPoMaterial
{

	std::vector<SPoDescriptorSetLayoutBinding> mDescriptorSetLayoutBindings;
	std::vector<SPoShaderLayout> mShaders;
	// TODO : get dynamic vertex attributes up and running and compare performance 
	// for now no models will have variance in vertex inputs so we can simply add it here for posterity
	std::vector<SPoVertexInputBindingDescription> mVertexBindingDescriptions;
	std::vector<SPoVertextInputAttributeDescription> mVertexInputAttributeDescriptions;
	VkPrimitiveTopology mTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkBool32 mPrimitiveRestartEnable = VK_FALSE;

	// intentionally leave out tessellation as it has become outdated because it's slow and largely unnecessary 

	SPoPipelineRasterizationStateCreateInfo mRasterizer;

	SPoPipelineMultisampleStateCreateInfo mMultisampling;

	SPoPipelineDepthStencilStateCreateInfo mDepthStencil;

	VkBool32 mColorBlendingOpEnable = VK_FALSE;
	VkLogicOp mColorBlendingOp = VK_LOGIC_OP_COPY;
	uint32_t mColorBlendingCount = 1;
	std::array<SPoPipelineColorBlendAttachmentState, 4> mColorBlendAttachments =
		// INTENTIONALLY INITIALIZE JUST THE ONE
	{
		SPoPipelineColorBlendAttachmentState{
			.mBlendEnable = VK_TRUE,
			.mSrcColorBlendFactor = VK_BLEND_FACTOR_ONE,
			.mDstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.mColorBlendOp = VK_BLEND_OP_ADD,
			.mSrcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.mDstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.mAlphaBlendOp = VK_BLEND_OP_ADD,
			.mColorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}
	};
	std::array<float, 4> mColorBlendConstants = { 0.0, 0.0, 0.0, 0.0 };
	// derived from descriptorSetLayout count uint32_t mLayoutCount = 1;
	// currently do not have push constants
	bool operator==(SPoMaterial const &) const = default;
	bool operator!=(SPoMaterial const &) const = default;

};

namespace NPoMaterialBehavior
{
}