#pragma once

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include "PoGLFW.h"

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
};

struct SPoSlangState
{
	int mCurrentGeneration = -1;
	int mShaderModuleGeneration[SPoSlangSettings::skMaxShaderModules] = {};
	VkDevice mDevice = VK_NULL_HANDLE;
	std::filesystem::file_time_type mLastWriteTime = std::filesystem::file_time_type();

};

struct SPoSlangResources
{
	Slang::ComPtr<slang::IGlobalSession> mpGlobalSession;
	Slang::ComPtr<slang::ISession> mpSessionInstance;

	VkShaderModule mVertexShaderModule[SPoSlangSettings::skMaxShaderModules] = {};
	VkShaderModule mFragmentShaderModule[SPoSlangSettings::skMaxShaderModules] = {};
};

namespace NPoSlangBehavior
{
	SPoSlangState init(SPoSlangResources &outResources, SPoSlangSettings const &settings, VkDevice device);
	void refresh_shaders(SPoSlangResources &inOutResources, SPoSlangState &inOutState, SPoSlangSettings const &settings);
	void get_current_shaders(VkShaderModule &outVertex, VkShaderModule &outFragment, SPoSlangState const &state, SPoSlangResources &resources);
	void cleanup(SPoSlangResources &inOutResources, SPoSlangState &inOutState, SPoSlangSettings const &settings);
}