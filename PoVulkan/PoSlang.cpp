#include "PoSlang.h"

#include <stdexcept>
#include <filesystem>
#include <vector>
#include <slang/slang-cpp-types-core.h>


namespace NPoSlangPrivate
{
	VkShaderModule create_shader_module(VkDevice device, Slang::ComPtr<slang::IBlob> &code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code->getBufferSize();
		createInfo.pCode = reinterpret_cast<const uint32_t *>(code->getBufferPointer());
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module!");
		}
		return shaderModule;
	}
}
SPoSlangState NPoSlangBehavior::init(SPoSlangResources &outResources, SPoSlangSettings const &settings, VkDevice device)
{
    SPoSlangState state = {};
	state.mDevice = device;

    auto result = slang::createGlobalSession(&(settings.mGlobalDescription), outResources.mpGlobalSession.writeRef());
    if (!SLANG_SUCCEEDED(result))
    {
	    throw std::runtime_error("failed to setup slang compilation!");
    }

    static char const *const skIncludePaths[] = { "shaders" };
    static char const *const skProfile = "spirv_1_5" ;
    slang::TargetDesc targetDescription =
        {
            .format = SLANG_SPIRV,
            .profile = outResources.mpGlobalSession->findProfile(skProfile),
            .flags = 0,
        };

    slang::SessionDesc instanceSessionDescription = slang::SessionDesc
    {
        /** The size of this structure, in bytes.
         */
        .structureSize = sizeof(slang::SessionDesc),

        /** Code generation targets to include in the session.
         */
        .targets = &targetDescription,
        .targetCount = 1,

        /** Flags to configure the session.
         */
        .flags = slang::kSessionFlags_None,

        /** Default layout to assume for variables with matrix types.
         */
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,

        /** Paths to use when searching for `#include`d or `import`ed files.
         */
        .searchPaths = skIncludePaths,
        .searchPathCount = sizeof(skIncludePaths) / sizeof(skIncludePaths[0]),

        .preprocessorMacros = nullptr,
        .preprocessorMacroCount = 0,

        .fileSystem = nullptr,

        .enableEffectAnnotations = false,
        .allowGLSLSyntax = false,

        /** Pointer to an array of compiler option entries, whose size is compilerOptionEntryCount.
         */
        .compilerOptionEntries = nullptr,

        /** Number of additional compiler option entries.
         */
        .compilerOptionEntryCount = 0,

        /** Whether to skip SPIRV validation.
         */
        .skipSPIRVValidation = false,
    };


    result = (outResources.mpGlobalSession->createSession(instanceSessionDescription, outResources.mpSessionInstance.writeRef()));
    if (!SLANG_SUCCEEDED(result))
    {
        throw std::runtime_error("failed to setup slang compilation!");
    }
	refresh_shaders(outResources, state, settings);

    return state;
}

void NPoSlangBehavior::refresh_shaders(SPoSlangResources &inOutResources, SPoSlangState &inOutState, SPoSlangSettings const &settings)
{
	using namespace NPoSlangPrivate;
	char const *slangShaderPath = "shaders/simple_shader.slang";
	std::filesystem::path slangShaderFilesystemPath = slangShaderPath;
	if (!std::filesystem::exists(slangShaderFilesystemPath))
	{
		return; // error
	}
	std::filesystem::file_time_type latestWriteTime = std::filesystem::last_write_time(slangShaderFilesystemPath);
	if (inOutState.mLastWriteTime != std::filesystem::file_time_type() && inOutState.mLastWriteTime != latestWriteTime)
	{
		return; // no refresh necessary
	}
	

	slang::IModule *slangModule = nullptr;
	{
		Slang::ComPtr<slang::IBlob> diagnosticBlob;
		slangModule = inOutResources.mpSessionInstance->loadModule(slangShaderPath, diagnosticBlob.writeRef());
		if (diagnosticBlob != nullptr)
		{
			printf("path <%s> load resulted in : %s", slangShaderPath, (char *)diagnosticBlob->getBufferPointer());
		}
		if (slangModule == nullptr)
		{
			return; // error
		}
	}
	Slang::ComPtr<slang::IEntryPoint> vertexEntryPoint;
	Slang::ComPtr<slang::IEntryPoint> fragmentEntryPoint;
	slangModule->getDefinedEntryPoint(0, vertexEntryPoint.writeRef());
	slangModule->getDefinedEntryPoint(1, fragmentEntryPoint.writeRef());

	std::vector<slang::IComponentType *> componentTypes;
	componentTypes.push_back(slangModule);
	componentTypes.push_back(vertexEntryPoint);
	componentTypes.push_back(fragmentEntryPoint);

	Slang::ComPtr<slang::IComponentType> composedProgram;
	{
		Slang::ComPtr<slang::IBlob> diagnosticBlob;
		SlangResult result = inOutResources.mpSessionInstance->createCompositeComponentType(
			componentTypes.data(),
			componentTypes.size(),
			composedProgram.writeRef(),
			diagnosticBlob.writeRef());
		if (diagnosticBlob != nullptr)
		{
			printf("slang compose program diagnostics reported : %s", (char *)diagnosticBlob->getBufferPointer());
		}
		if (!SLANG_SUCCEEDED(result))
		{
			return; // error
		}
	}

	Slang::ComPtr<slang::IBlob> spirvCode;
	Slang::ComPtr<slang::IBlob> spirvCode2;
	{

		int spirIndex = 0;
		for (auto spir : { spirvCode.writeRef(), spirvCode2.writeRef() })
		{
			Slang::ComPtr<slang::IBlob> diagnosticBlob;
			SlangResult result = composedProgram->getEntryPointCode(
				spirIndex,
				0,
				spir,
				diagnosticBlob.writeRef()
			);
			spirIndex++;
			if (diagnosticBlob != nullptr)
			{
				printf("slang compose program diagnostics reported : %s", (char *)diagnosticBlob->getBufferPointer());
			}
			if (!SLANG_SUCCEEDED(result))
			{
				return; // error
			}
		}
	}
	inOutState.mCurrentGeneration = (inOutState.mCurrentGeneration + 1);
	int const index = inOutState.mCurrentGeneration % SPoSlangSettings::skMaxShaderModules;
	VkShaderModule &vertexModule = inOutResources.mVertexShaderModule[index];
	VkShaderModule &fragmentModule = inOutResources.mFragmentShaderModule[index];
	if (inOutState.mCurrentGeneration > SPoSlangSettings::skMaxShaderModules)
	{
		vkDestroyShaderModule(inOutState.mDevice, vertexModule, nullptr);
		vkDestroyShaderModule(inOutState.mDevice, fragmentModule, nullptr);
	}
	vertexModule = create_shader_module(inOutState.mDevice, spirvCode);
	fragmentModule = create_shader_module(inOutState.mDevice, spirvCode2);
	inOutState.mLastWriteTime = latestWriteTime;
}

void NPoSlangBehavior::get_current_shaders(VkShaderModule &outVertex, VkShaderModule &outFragment, SPoSlangState const &state, SPoSlangResources &resources)
{
	if (state.mCurrentGeneration < 0)
	{
		throw std::runtime_error("failed to get currrent shaders due to generation being negative!");
	}
	int const index = state.mCurrentGeneration % SPoSlangSettings::skMaxShaderModules;
	outVertex = resources.mVertexShaderModule[index];
	outFragment = resources.mFragmentShaderModule[index];
}

void NPoSlangBehavior::cleanup(SPoSlangResources &inOutResources, SPoSlangState &inOutState, SPoSlangSettings const &settings)
{
	int const num = inOutState.mCurrentGeneration > SPoSlangSettings::skMaxShaderModules ? SPoSlangSettings::skMaxShaderModules : (inOutState.mCurrentGeneration + 1);
	for (int i = 0; i < num; ++i)
	{
		vkDestroyShaderModule(inOutState.mDevice, inOutResources.mVertexShaderModule[i], nullptr);
		vkDestroyShaderModule(inOutState.mDevice, inOutResources.mFragmentShaderModule[i], nullptr);
	}
	inOutResources = {};
	inOutState = {};
}