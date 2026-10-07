#include "PoSlang.h"
#include "PoMaterial.h"

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

	int get_index_of_shader(std::vector<std::filesystem::path> const &paths, std::filesystem::path const &path)
	{
		int shaderIndex = 0;
		for (; shaderIndex < paths.size(); ++shaderIndex)
		{
			if (paths[shaderIndex] == path)
			{
				return shaderIndex;
			}
		}
		return -1;
	}

	SlangResult refresh_session(Slang::ComPtr<slang::ISession> &pInOutSessionInstance, Slang::ComPtr<slang::IGlobalSession> const &pGlobalSession)
	{
		static char const *const skIncludePaths[] = { "shaders" };
		static char const *const skProfile = "spirv_1_5";
		slang::TargetDesc targetDescription =
		{
			.format = SLANG_SPIRV,
			.profile = pGlobalSession->findProfile(skProfile),
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
		return (pGlobalSession->createSession(instanceSessionDescription, pInOutSessionInstance.writeRef()));
	}

	slang::IModule *load_module(Slang::ComPtr<slang::ISession> &pSessionInstance, char const *slangShaderPath)
	{
		slang::IModule *pSlangModule = nullptr;
		{
			Slang::ComPtr<slang::IBlob> diagnosticBlob;
			pSlangModule = pSessionInstance->loadModule(slangShaderPath, diagnosticBlob.writeRef());
			if (diagnosticBlob != nullptr)
			{
				printf("path <%s> load resulted in : %s", slangShaderPath, (char *)diagnosticBlob->getBufferPointer());
			}
			if (pSlangModule == nullptr)
			{
				return nullptr; // error
			}
		}
		return pSlangModule;
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


	refresh_shaders(outResources, state, settings);

    return state;
}

bool NPoSlangBehavior::refresh_compute_shader(SPoSlangResources &inOutResources, SPoSlangState &inOutState, char const *slangShaderPath)
{
	using namespace NPoSlangPrivate;

	std::filesystem::path slangShaderFilesystemPath = slangShaderPath;
	int shaderIndex = get_index_of_shader(inOutState.mComputeCollection.mPaths, slangShaderFilesystemPath);
	bool isExistingShader = shaderIndex >= 0;

	if (!std::filesystem::exists(slangShaderFilesystemPath))
	{
		return false; // error
	}
	std::filesystem::file_time_type latestWriteTime = std::filesystem::last_write_time(slangShaderFilesystemPath);
	if (isExistingShader && inOutState.mComputeCollection.mUpdateInfo[shaderIndex].mTime == latestWriteTime)
	{
		return false; // no refresh necessary
	}
	// shader is newer so refresh the session to clear cache
	if (isExistingShader || inOutResources.mpSessionInstance.get() == nullptr)
	{
		auto result = refresh_session(inOutResources.mpSessionInstance, inOutResources.mpGlobalSession);
		if (!SLANG_SUCCEEDED(result))
		{
			throw std::runtime_error("failed to setup slang compilation!");
		}
	}

	slang::IModule *slangModule = load_module(inOutResources.mpSessionInstance, slangShaderPath);
	if (slangModule == nullptr)
	{
		return false;
	}
	Slang::ComPtr<slang::IEntryPoint> computeEntryPoint;
	slangModule->getDefinedEntryPoint(0, computeEntryPoint.writeRef());

	std::vector<slang::IComponentType *> componentTypes;
	componentTypes.push_back(slangModule);
	componentTypes.push_back(computeEntryPoint);

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
			return false;
		}
	}

	Slang::ComPtr<slang::IBlob> spirvCode;
	{

		int spirIndex = 0;
		for (auto spir : { spirvCode.writeRef() })
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
				return false;
			}
		}
	}
	if (!isExistingShader)
	{
		inOutResources.mComputeModules.emplace_back();
		inOutState.mComputeCollection.mPaths.push_back(slangShaderFilesystemPath);
		inOutState.mComputeCollection.mUpdateInfo.push_back({ .mTime = latestWriteTime, .mCurrentGeneration = 0 });
		shaderIndex = static_cast<int>(inOutResources.mComputeModules.size() - 1);
	}
	else
	{
		inOutState.mComputeCollection.mUpdateInfo[shaderIndex].mTime = latestWriteTime;
		inOutState.mComputeCollection.mUpdateInfo[shaderIndex].mCurrentGeneration++;
	}
	int const generationIndex = inOutState.mComputeCollection.mUpdateInfo[shaderIndex].mCurrentGeneration % SPoSlangSettings::skMaxShaderModules;
	VkShaderModule &computeModule = inOutResources.mComputeModules[shaderIndex].mShaderModules[generationIndex];
	Slang::ComPtr<slang::IComponentType> &pProgram = inOutResources.mComputeModules[shaderIndex].mPrograms[generationIndex];
	if (inOutState.mComputeCollection.mUpdateInfo[shaderIndex].mCurrentGeneration >= SPoSlangSettings::skMaxShaderModules)
	{
		vkDestroyShaderModule(inOutState.mDevice, computeModule, nullptr);
		pProgram.setNull();
	}
	computeModule = create_shader_module(inOutState.mDevice, spirvCode);
	pProgram = std::move(composedProgram);
	return true;
}

bool NPoSlangBehavior::refresh_shader(SPoSlangResources &inOutResources, SPoSlangState &inOutState, char const *slangShaderPath)
{
	using namespace NPoSlangPrivate;
	
	std::filesystem::path slangShaderFilesystemPath = slangShaderPath;
	int shaderIndex = get_index_of_shader(inOutState.mShaderCollection.mPaths, slangShaderFilesystemPath);
	bool isExistingShader = shaderIndex >= 0;

	if (!std::filesystem::exists(slangShaderFilesystemPath))
	{
		return false; // error
	}
	std::filesystem::file_time_type latestWriteTime = std::filesystem::last_write_time(slangShaderFilesystemPath);
	if (isExistingShader && inOutState.mShaderCollection.mUpdateInfo[shaderIndex].mTime == latestWriteTime)
	{
		return false; // no refresh necessary
	}
	// shader is newer so refresh the session to clear cache
	if (isExistingShader || inOutResources.mpSessionInstance.get() == nullptr)
	{
		auto result = refresh_session(inOutResources.mpSessionInstance, inOutResources.mpGlobalSession);
		if (!SLANG_SUCCEEDED(result))
		{
			throw std::runtime_error("failed to setup slang compilation!");
		}
	}

	slang::IModule *slangModule = load_module(inOutResources.mpSessionInstance, slangShaderPath);
	if (slangModule == nullptr)
	{
		return false;
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
			return false;
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
				return false;
			}
		}
	}
	if (!isExistingShader)
	{
		inOutResources.mShaderModules.emplace_back();
		inOutState.mShaderCollection.mPaths.push_back(slangShaderFilesystemPath);
		inOutState.mShaderCollection.mUpdateInfo.push_back({.mTime = latestWriteTime, .mCurrentGeneration = 0});
		shaderIndex = static_cast<int>(inOutResources.mShaderModules.size() - 1);
	}
	else
	{
		inOutState.mShaderCollection.mUpdateInfo[shaderIndex].mTime = latestWriteTime;
		inOutState.mShaderCollection.mUpdateInfo[shaderIndex].mCurrentGeneration++;
	}
	int const generationIndex = inOutState.mShaderCollection.mUpdateInfo[shaderIndex].mCurrentGeneration % SPoSlangSettings::skMaxShaderModules;
	VkShaderModule &vertexModule = inOutResources.mShaderModules[shaderIndex].mVertexShaderModule[generationIndex];
	VkShaderModule &fragmentModule = inOutResources.mShaderModules[shaderIndex].mFragmentShaderModule[generationIndex];
	Slang::ComPtr<slang::IComponentType> &pProgram = inOutResources.mShaderModules[shaderIndex].mPrograms[generationIndex];
	if (inOutState.mShaderCollection.mUpdateInfo[shaderIndex].mCurrentGeneration >= SPoSlangSettings::skMaxShaderModules)
	{
		vkDestroyShaderModule(inOutState.mDevice, vertexModule, nullptr);
		vkDestroyShaderModule(inOutState.mDevice, fragmentModule, nullptr);
		pProgram.setNull();
	}
	vertexModule = create_shader_module(inOutState.mDevice, spirvCode);
	fragmentModule = create_shader_module(inOutState.mDevice, spirvCode2);
	pProgram = std::move(composedProgram);
	return true;
}

bool NPoSlangBehavior::refresh_shaders(SPoSlangResources &inOutResources, SPoSlangState &inOutState, SPoSlangSettings const &settings)
{
	using namespace NPoSlangPrivate;
	// make config that has each shader path
	char const *slangShaderPath = settings.mSlangDefaultShader;
	return refresh_shader(inOutResources, inOutState, slangShaderPath);
}

void NPoSlangBehavior::get_current_compute_shader(VkShaderModule &outCompute, Slang::ComPtr<slang::IComponentType> &pOutCompositeProgram, SPoSlangState const &state, SPoSlangResources &resources, char const *pShaderFile)
{
	using namespace NPoSlangPrivate;
	int const shaderIndex = get_index_of_shader(state.mComputeCollection.mPaths, pShaderFile);
	bool const isExistingShader = shaderIndex >= 0;
	if (!isExistingShader || state.mComputeCollection.mUpdateInfo[shaderIndex].mCurrentGeneration < 0)
	{
		throw std::runtime_error("failed to get currrent shader!");
	}
	int const index = state.mComputeCollection.mUpdateInfo[shaderIndex].mCurrentGeneration % SPoSlangSettings::skMaxShaderModules;
	outCompute = resources.mComputeModules[shaderIndex].mShaderModules[index];
	pOutCompositeProgram = resources.mComputeModules[shaderIndex].mPrograms[index];
}

void NPoSlangBehavior::get_current_shaders(VkShaderModule &outVertex, VkShaderModule &outFragment, SPoSlangState const &state, SPoSlangResources &resources, char const *pShaderFile)
{
	using namespace NPoSlangPrivate;
	int const shaderIndex = get_index_of_shader(state.mShaderCollection.mPaths, pShaderFile);
	bool const isExistingShader = shaderIndex >= 0;
	if (!isExistingShader || state.mShaderCollection.mUpdateInfo[shaderIndex].mCurrentGeneration < 0)
	{
		throw std::runtime_error("failed to get currrent shader!");
	}
	int const index = state.mShaderCollection.mUpdateInfo[shaderIndex].mCurrentGeneration % SPoSlangSettings::skMaxShaderModules;
	outVertex = resources.mShaderModules[shaderIndex].mVertexShaderModule[index];
	outFragment = resources.mShaderModules[shaderIndex].mFragmentShaderModule[index];
}

void NPoSlangBehavior::get_current_default_shaders(VkShaderModule &outVertex, VkShaderModule &outFragment, SPoSlangState const &state, SPoSlangResources &resources)
{
	if (state.mShaderCollection.mUpdateInfo[0].mCurrentGeneration < 0)
	{
		throw std::runtime_error("failed to get currrent shaders due to generation being negative!");
	}
	int const index = state.mShaderCollection.mUpdateInfo[0].mCurrentGeneration % SPoSlangSettings::skMaxShaderModules;
	outVertex = resources.mShaderModules[0].mVertexShaderModule[index];
	outFragment = resources.mShaderModules[0].mFragmentShaderModule[index];
}

void NPoSlangBehavior::cleanup(SPoSlangResources &inOutResources, SPoSlangState &inOutState, SPoSlangSettings const &settings)
{

	for (int shaderIndex = 0; shaderIndex < inOutState.mShaderCollection.mPaths.size(); ++shaderIndex)
	{
		int const num = inOutState.mShaderCollection.mUpdateInfo[shaderIndex].mCurrentGeneration >= SPoSlangSettings::skMaxShaderModules ? SPoSlangSettings::skMaxShaderModules : (inOutState.mShaderCollection.mUpdateInfo[shaderIndex].mCurrentGeneration + 1);
		for (int i = 0; i < num; ++i)
		{
			vkDestroyShaderModule(inOutState.mDevice, inOutResources.mShaderModules[shaderIndex].mVertexShaderModule[i], nullptr);
			vkDestroyShaderModule(inOutState.mDevice, inOutResources.mShaderModules[shaderIndex].mFragmentShaderModule[i], nullptr);
			inOutResources.mShaderModules[shaderIndex].mPrograms[i].setNull();
		}
	}
	for (int shaderIndex = 0; shaderIndex < inOutState.mComputeCollection.mPaths.size(); ++shaderIndex)
	{
		int const num = inOutState.mComputeCollection.mUpdateInfo[shaderIndex].mCurrentGeneration >= SPoSlangSettings::skMaxShaderModules ? SPoSlangSettings::skMaxShaderModules : (inOutState.mComputeCollection.mUpdateInfo[shaderIndex].mCurrentGeneration + 1);
		for (int i = 0; i < num; ++i)
		{
			vkDestroyShaderModule(inOutState.mDevice, inOutResources.mComputeModules[shaderIndex].mShaderModules[i], nullptr);
			inOutResources.mShaderModules[shaderIndex].mPrograms[i].setNull();
		}
	}
	inOutResources = {};
	inOutState = {};
}

