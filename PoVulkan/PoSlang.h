#pragma once

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include "PoGLFW.h"
#include "PoMaterial.h"

#include <string>
#include <vector>
#include <filesystem>

struct SPoSlangSettings
{
	static int const skMaxShaderModules = 3;
	SlangGlobalSessionDesc mGlobalDescription = SlangGlobalSessionDesc
		{
			/// Size of this struct.
			.structureSize = sizeof(SlangGlobalSessionDesc),

			/// Slang API version.
			.apiVersion = SLANG_API_VERSION,

			/// Specify the oldest Slang language version that any sessions will use.
			.minLanguageVersion = SLANG_LANGUAGE_VERSION_2025,

			/// Whether to enable GLSL support.
			.enableGLSL = false,
		};
	char mSlangDefaultShader[28] = "shaders/simple_shader.slang";
};

struct SPoShaderUpdateInfo
{
	std::filesystem::file_time_type mTime = {};
	int mCurrentGeneration = -1;
};

// Plan :
// Pull out mShaders and mShaderUpdateInfo into it's own struct
// Then have an instance of that struct for Vertex + Fragment
// Then have an instance for compute shaders specifically
// Compute shaders don't need a wrapper struct they can just be a vector of arrays
// Then we will add an explicit set of functions for them
// Why don't we make an explicit namespace for them
// Then we can move their contents into dedicated structs kinda like how they are already going to for state.

struct SPoShaderCollectionState
{
	std::vector<std::filesystem::path> mPaths = {};
	std::vector<SPoShaderUpdateInfo> mUpdateInfo = {};
};

struct SPoSlangState
{
	VkDevice mDevice = VK_NULL_HANDLE;
	SPoShaderCollectionState mShaderCollection = {};
	SPoShaderCollectionState mComputeCollection = {};
};

struct SPoSlangShaderModules
{
	VkShaderModule mVertexShaderModule[SPoSlangSettings::skMaxShaderModules] = {};
	VkShaderModule mFragmentShaderModule[SPoSlangSettings::skMaxShaderModules] = {};
	Slang::ComPtr<slang::IComponentType> mPrograms[SPoSlangSettings::skMaxShaderModules] = {};
};

struct SPoSlangComputeModules
{
	std::array<VkShaderModule, SPoSlangSettings::skMaxShaderModules> mShaderModules;
	std::array<Slang::ComPtr<slang::IComponentType>, SPoSlangSettings::skMaxShaderModules> mPrograms = {};
};

struct SPoSlangResources
{
	Slang::ComPtr<slang::IGlobalSession> mpGlobalSession;
	Slang::ComPtr<slang::ISession> mpSessionInstance;
	std::vector<SPoSlangShaderModules> mShaderModules;
	std::vector<SPoSlangComputeModules> mComputeModules;
};

// maybe write higher level thing that gets converted to slang
// slang shaders
// compile slang shaders
// reflect info from slang shaders
// build pipeline from slang shaders + additional settings info

namespace NPoSlangBehavior
{
	SPoSlangState init(SPoSlangResources &outResources, SPoSlangSettings const &settings, VkDevice device);
	bool refresh_compute_shader(SPoSlangResources &inOutResources, SPoSlangState &inOutState, char const *slangShaderPath);
	bool refresh_shaders(SPoSlangResources &inOutResources, SPoSlangState &inOutState, SPoSlangSettings const &settings);
	bool refresh_shader(SPoSlangResources &inOutResources, SPoSlangState &inOutState, char const *slangShaderPath);
	void get_current_compute_shader(VkShaderModule &outCompute, Slang::ComPtr<slang::IComponentType> &pOutCompositeProgram, SPoSlangState const &state, SPoSlangResources &resources, char const *pShaderFile);
	void get_current_shaders(VkShaderModule &outVertex, VkShaderModule &outFragment, SPoSlangState const &state, SPoSlangResources &resources, char const *pShaderFile);
	void get_current_default_shaders(VkShaderModule &outVertex, VkShaderModule &outFragment, SPoSlangState const &state, SPoSlangResources &resources);
	void cleanup(SPoSlangResources &inOutResources, SPoSlangState &inOutState, SPoSlangSettings const &settings);
}