#include "PoVulkanDevice.h"
#include "PoVulkanSwapchain.h"
#include "PoGlm.h"
#include "PoMesh.h"
#include "PoSlang.h"


#include <stdexcept>
#include <set>
#include <map>
#include <array>
#include <ios>
#include <fstream>
#include <slang/slang-cpp-types-core.h>


/* Implementation for vulkan device behavior.
*	Vulkan logical devices are the ambassador of vulkkan resources so much of the core vulkan implementation resides here.
*	
*	Quirks : I didn't think of a good way to separate the glfw window and the vulkan logical device.
*			The glfw window resizing relies on binding a memory address of ours to fetch inside a resize callback.
*			
*	Coding Style : I prefer to keep data explicit by requiring all data in a function to be explicitly passed to it.
*		This results in larger function signatures. This means that when function signatures change they require updating 
*		many call sites. The bonus is that by keeping data explicit, it makes it much easier to rework where and how data is managed.
*	
*	resources used to help implement : 
*	https://vulkan-tutorial.com/
*	https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine
*/


int SPoWindowId::sUniqueIdSeed = 0;

namespace NPoVulkanDevicePrivate
{
	void create_surface(VkSurfaceKHR &outSurface, VkInstance instance, GLFWwindow *pWindow)
	{
		if (glfwCreateWindowSurface(instance, pWindow, nullptr, &outSurface) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface!");
		}
	}

	bool check_device_extension_support(VkPhysicalDevice device, std::vector<const char *> const &deviceExtensions)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto &extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	bool is_device_suitable(VkSurfaceKHR surface, VkPhysicalDevice device, SPoVulkanDeviceSettings const &settings)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		SQueueFamilyIndices indices = NPoVulkanDeviceBehavior::find_queue_families(surface, device);

		bool extensionSupported = check_device_extension_support(device, settings.mDeviceExtensions);

		bool swapChainAdequate = false;
		if (extensionSupported)
		{
			SSwapchainSupportDetails swapChainSupport = NPoVulkanSwapchainBehavior::query_swap_chain_support(surface, device);
			swapChainAdequate = !swapChainSupport.mFormats.empty() && !swapChainSupport.mPresentModes.empty();
		}

		return indices.isComplete() && extensionSupported && swapChainAdequate && deviceFeatures.samplerAnisotropy;
	}

	int rate_device_suitability(VkSurfaceKHR surface, VkPhysicalDevice device, SPoVulkanDeviceSettings const &settings)
	{

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		int score = 0;

		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		score += deviceProperties.limits.maxImageDimension2D;

		if (!deviceFeatures.geometryShader)
		{
			return 0;
		}

		if (!is_device_suitable(surface, device, settings))
		{
			return 0;
		}

		return score;
	}

	VkSampleCountFlagBits get_max_usable_sample_count(VkPhysicalDevice &physicalDevice)
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

		if (counts & VK_SAMPLE_COUNT_64_BIT)
		{
			return VK_SAMPLE_COUNT_64_BIT;
		}
		if (counts & VK_SAMPLE_COUNT_32_BIT)
		{
			return VK_SAMPLE_COUNT_32_BIT;
		}
		if (counts & VK_SAMPLE_COUNT_16_BIT)
		{
			return VK_SAMPLE_COUNT_16_BIT;
		}
		if (counts & VK_SAMPLE_COUNT_8_BIT)
		{
			return VK_SAMPLE_COUNT_8_BIT;
		}
		if (counts & VK_SAMPLE_COUNT_4_BIT)
		{
			return VK_SAMPLE_COUNT_4_BIT;
		}
		if (counts & VK_SAMPLE_COUNT_2_BIT)
		{
			return VK_SAMPLE_COUNT_2_BIT;
		}
		return VK_SAMPLE_COUNT_1_BIT;
	}

	VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface, SPoVulkanDeviceSettings const &settings)
	{
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		std::multimap<int, VkPhysicalDevice> candidates;

		for (const auto &device : devices)
		{
			if (int score = rate_device_suitability(surface, device, settings))
			{
				candidates.insert({ score, device });
				break;
			}
		}

		if (candidates.size() > 0)
		{
			physicalDevice = candidates.rbegin()->second;
			// TODO : add this back
			// msaaSamples = getMaxUsableSampleCount(inOutResources);
		}

		if (physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
		return physicalDevice;
	}

	void create_logical_device(VkDevice &outLogicalDevice, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings, SQueueFamilyIndices const &indices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;


		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;


		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>(settings.mDeviceExtensions.size());
		createInfo.ppEnabledExtensionNames = settings.mDeviceExtensions.data();

		VkPhysicalDeviceVulkan13Features vulkanFeatures13 = {};
		vulkanFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vulkanFeatures13.synchronization2 = true;
		vulkanFeatures13.dynamicRendering = true;

		VkPhysicalDeviceVulkan12Features vulkanFeatures12 = {};
		vulkanFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vulkanFeatures12.pNext = &vulkanFeatures13;
		vulkanFeatures12.runtimeDescriptorArray = VK_TRUE;
		vulkanFeatures12.descriptorIndexing = VK_TRUE;
		vulkanFeatures12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		vulkanFeatures12.descriptorBindingVariableDescriptorCount = VK_TRUE;
		vulkanFeatures12.bufferDeviceAddress = VK_TRUE;
		VkPhysicalDeviceVulkan11Features vulkanFeatures11 = {};
		vulkanFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		vulkanFeatures11.pNext = &vulkanFeatures12;
		createInfo.pNext = &vulkanFeatures11;

		// this is deprecated so its set to zero
		createInfo.enabledLayerCount = 0;

		if (vkCreateDevice(inOutState.mPhysicalDevice, &createInfo, nullptr, &outLogicalDevice) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(outLogicalDevice, indices.graphicsFamily.value(), 0, &inOutState.mGraphicsQueue);

		vkGetDeviceQueue(outLogicalDevice, indices.presentFamily.value(), 0, &inOutState.mPresentQueue);
	}

	VkFormat find_supported_format(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	VkFormat find_depth_format(VkPhysicalDevice physicalDevice)
	{
		return find_supported_format(physicalDevice,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	void create_render_pass(VkRenderPass &outRenderPass, VkDevice logicalDevice, VkSampleCountFlagBits msaaSamples, VkFormat swapChainImageFormat, VkFormat const depthFormat)
	{
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = msaaSamples;

		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = swapChainImageFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;

		std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &outRenderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render pass!");
		}

	}

	void create_descriptor_set_layout(VkDescriptorSetLayout &outDescriptorSetLayout, VkDevice device)
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &outDescriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor set layout!");
		}

	}

	static std::vector<char> read_file(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}

	VkShaderModule create_shader_module(VkDevice device, std::vector<char> const &code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module!");
		}
		return shaderModule;
	}

	void create_graphics_pipeline(VkPipelineLayout &outPipelineLayout, VkPipeline &outGraphicsPipeline, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkRenderPass renderPass, VkExtent2D const &extent, VkSampleCountFlagBits const msaaSamples, SPoSlangState const &slangState, SPoSlangResources &slangResources)
	{
		VkShaderModule slangVert;
		VkShaderModule slangFrag;
		NPoSlangBehavior::get_current_shaders(slangVert, slangFrag, slangState, slangResources);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = slangVert;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = slangFrag;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)extent.width;
		viewport.height = (float)extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_TRUE;
		multisampling.rasterizationSamples = msaaSamples;
		multisampling.minSampleShading = 0.2f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_TRUE;
		depthStencil.front = {};
		depthStencil.back = {};

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &outPipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;

		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = outPipelineLayout;

		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outGraphicsPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create graphics pipeline");
		}
	}
	void create_command_pool(VkCommandPool &outCommandPool, VkDevice device, SQueueFamilyIndices const &indices)
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &outCommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}
	}


	uint32_t find_memory_type(VkPhysicalDeviceMemoryProperties const &memoryProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
		{
			if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("failed to find suitable memory type!");
		return UINT32_MAX;
	}


	VkCommandBuffer begin_single_time_commands(VkDevice device, VkCommandPool commandPool)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void end_single_time_commands(VkCommandBuffer &inOutommandBuffer, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue)
	{
		vkEndCommandBuffer(inOutommandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &inOutommandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(device, commandPool, 1, &inOutommandBuffer);
	}

	bool has_stencil_component(VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void create_descriptor_pool(VkDescriptorPool &outDescriptorPool, VkDevice device, int const descriptorCount)
	{
		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(descriptorCount);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(descriptorCount);
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(descriptorCount);

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &outDescriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void create_command_buffers(std::vector<VkCommandBuffer> &outCommandBuffers, VkDevice device, VkCommandPool commandPool, int const imageCount)
	{
		outCommandBuffers.resize(imageCount);
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)outCommandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, outCommandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}


	void create_sync_objects(std::vector<VkSemaphore> &outImageAvailableSemaphores, std::vector<VkSemaphore> &outRenderFinishedSemaphores, std::vector<VkFence> &outInFlightFences, VkDevice device, int const imageCount)
	{
		outImageAvailableSemaphores.resize(imageCount);
		outRenderFinishedSemaphores.resize(imageCount);
		outInFlightFences.resize(imageCount);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < imageCount; ++i)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &outImageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &outRenderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &outInFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create semaphores!");
			}
		}
	}
	/*
	* so for something like deferred pbr rendering...
	* an object gets a pipeline for rendering normals, roughness/metalness, albedo, emissive, and depth
	* then a final pipeline is used to render the combo of these things with lighting
	* 
	*/
	// feel like this should be called create render objects then make the vulkan game objects which will make their respective
	// uniform buffers and descriptor sets
	

	/* TODO : replace these
	(x) createUniformBuffer(inOutResources); => being done via game objects though would like to move it back over here
	(x) createDescriptorSets(inOutResources); => being done via game objects though would like to move it back over here
	(x) createCommandBuffers(inOutResources);*/


	void record_command_buffer(VkCommandBuffer commandBuffer, int imageIndex, int currentFrame,
		VkRenderPass renderPass, std::vector<VkFramebuffer> const &swapChainFramebuffers, VkExtent2D const &swapChainExtent,
		VkPipeline graphicsPipeline, VkPipelineLayout pipelineLayout, SPoMeshResources const &meshResources, 
		std::array<SPoGameObjectResources, NPoGameObjectBehavior::gk_max_game_objects> &gameObjectsResources,
		std::array<SPoGameObjectState, NPoGameObjectBehavior::gk_max_game_objects> &gameObjectsState)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainExtent;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkBuffer vertexBuffers[] = { meshResources.mVertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		vkCmdBindIndexBuffer(commandBuffer, meshResources.mIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		for (int i = 0; i < NPoGameObjectBehavior::gk_max_game_objects; ++i)
		{
			
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &(gameObjectsResources[i].mDescriptorSets[currentFrame]), 0, nullptr);
			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshResources.mIndices.size()), 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record command buffer!");
		}

	}
}

void NPoVulkanDeviceBehavior::transition_image_layout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
{
	using namespace NPoVulkanDevicePrivate;
	VkCommandBuffer commandBuffer = begin_single_time_commands(device, commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;


	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (has_stencil_component(format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
		newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
		newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::runtime_error("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	end_single_time_commands(commandBuffer, device, commandPool, graphicsQueue);
}


void NPoVulkanDeviceBehavior::create_image(VkImage &outImage, VkDeviceMemory &outImageMemory, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties, VkPhysicalDeviceMemoryProperties const &memoryProperties)
{
	using namespace NPoVulkanDevicePrivate;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = numSamples;
	imageInfo.flags = 0;

	if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &outImage) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(logicalDevice, outImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type(memoryProperties, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &outImageMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(logicalDevice, outImage, outImageMemory, 0);
}

VkImageView NPoVulkanDeviceBehavior::create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;

	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image view!");
	}

	return imageView;
}



SQueueFamilyIndices NPoVulkanDeviceBehavior::find_queue_families(VkSurfaceKHR surface, VkPhysicalDevice device)
{
	SQueueFamilyIndices indices;
	// Logic to find graphics queue family
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto &queueFamily : queueFamilies)
	{
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = i;
		}
		if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			indices.graphicsFamily = i;
		}
		++i;
	}
	return indices;
}

void NPoVulkanDeviceBehavior::create_buffer(VkBuffer &inOutBuffer, VkDeviceMemory &inOutBufferMemory, VkDevice device, VkDeviceSize const size, VkBufferUsageFlags const usage, VkMemoryPropertyFlags const properties, VkPhysicalDeviceMemoryProperties const &memoryProperties)
{
	using namespace NPoVulkanDevicePrivate;
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &inOutBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, inOutBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type(memoryProperties, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &inOutBufferMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(device, inOutBuffer, inOutBufferMemory, 0);
}

void NPoVulkanDeviceBehavior::copy_buffer_to_image(VkImage image, VkBuffer buffer, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, uint32_t const width, uint32_t const height)
{
	using namespace NPoVulkanDevicePrivate;

	VkCommandBuffer commandBuffer = begin_single_time_commands(device, commandPool);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1 };

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);
	end_single_time_commands(commandBuffer, device, commandPool, graphicsQueue);
}



void NPoVulkanDeviceBehavior::generate_mipmaps(VkImage image, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, 
	VkFormat const imageFormat, int32_t const texWidth, int32_t const texHeight, uint32_t const mipLevels)
{
	using namespace NPoVulkanDevicePrivate;
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("texture image format does not support linear blitting!");
	}


	VkCommandBuffer commandBuffer = begin_single_time_commands(device, commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; ++i)
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0,0,0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0,0,0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;


	}
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	end_single_time_commands(commandBuffer, device, commandPool, graphicsQueue);
}


void NPoVulkanDeviceBehavior::copy_buffer(VkBuffer dstBuffer, VkBuffer srcBuffer, VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkDeviceSize const deviceSize)
{
	using namespace NPoVulkanDevicePrivate;
	VkCommandBuffer commandBuffer = begin_single_time_commands(device, commandPool);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = deviceSize;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	end_single_time_commands(commandBuffer, device, commandPool, graphicsQueue);
}

static void framebufferResizeCallback(GLFWwindow *pWindow, int width, int height)
{
	if (void *pPointer = glfwGetWindowUserPointer(pWindow))
	{
		SPoWindowResizeCommand &command = *reinterpret_cast<SPoWindowResizeCommand *>(pPointer);
		for (int windowIndex = 0; windowIndex < command.mpState->mWindowStateVector.size(); ++windowIndex)
		{
			if (command.mpState->mWindowStateVector[windowIndex].mpWindow == pWindow)
			{
				command.mpState->mWindowStateVector[windowIndex].mFramebufferResized = true;
			}
		}
	}
}

SPoVulkanDeviceState NPoVulkanDeviceBehavior::init(SPoVulkanDeviceResources &outResources, SPoVulkanDeviceSettings const &settings, VkInstance instance, GLFWwindow *pWindow)
{
	using namespace NPoVulkanDevicePrivate;
	SPoVulkanDeviceState state;
	state.mInstance = instance;
	add_window(outResources, state, settings, pWindow);
	return state;
}

void NPoVulkanDeviceBehavior::cleanup(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings)
{

	NPoMeshBehavior::cleanup(inOutResources.mMesh, inOutState.mMesh, settings.mMesh);

	NPoTextureBehavior::cleanup(inOutResources.mTexture, inOutState.mTexture, settings.mTexture);

	for (size_t i = inOutResources.mWindowResourcesVector.size(); i > 0;)
	{
		--i;
		NPoVulkanDeviceBehavior::close_window(inOutResources, inOutState, settings, inOutState.mWindowStateVector[i].mpWindow);
	}

	vkDestroyCommandPool(inOutResources.mLogicalDevice, inOutResources.mCommandPool, nullptr);

	NPoSlangBehavior::cleanup(inOutResources.mSlang, inOutState.mSlang, settings.mSlang);
	
	vkDestroyDevice(inOutResources.mLogicalDevice, nullptr);
}

void NPoVulkanDeviceBehavior::recreate_swap_chains_for_resize(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings, GLFWwindow *pWindow)
{
	//int width = 0, height = 0;
	//glfwGetFramebufferSize(inOutState.mpWindow, &width, &height);
	//// TODO : replace this hack
	//while (width == 0 || height == 0)
	//{
	//	glfwGetFramebufferSize(inOutState.mpWindow, &width, &height);
	//	glfwWaitEvents();
	//}
	vkDeviceWaitIdle(inOutResources.mLogicalDevice);
	
	size_t windowIndex = inOutState.mWindowStateVector.size();
	while (windowIndex-- > 0)
	{
		if (inOutState.mWindowStateVector[windowIndex].mpWindow == pWindow)
		{
			break;
		}
	}
	if (windowIndex >= 0)
	{
		SPoVulkanWindowState &windowState = inOutState.mWindowStateVector[windowIndex];
		SPoVulkanWindowResources &windowResources = inOutResources.mWindowResourcesVector[windowIndex];
		SPoVulkanWindowSettings const &windowSettings = settings.mWindowsSettings;
		windowState.mSwapchainSupportDetails = NPoVulkanSwapchainBehavior::query_swap_chain_support(windowResources.mSurface, inOutState.mPhysicalDevice);
		NPoVulkanSwapchainBehavior::cleanup(windowResources.mSwapchain, windowState.mSwapchain, windowSettings.mSwapchain);

		VkExtent2D const &extent = NPoVulkanSwapchainBehavior::choose_swap_extent(windowState.mpWindow, windowState.mSwapchainSupportDetails.mCapabilities);
		windowState.mSwapchain = NPoVulkanSwapchainBehavior::init(windowResources.mSwapchain, windowResources.mSurface, inOutResources.mLogicalDevice, windowResources.mCommandPool, inOutState.mGraphicsQueue, windowResources.mRenderPass,
			windowSettings.mSwapchain, windowState.mSwapchainSupportDetails, inOutState.mQueueFamilyIndices, extent, inOutState.mSurfaceFormat, inOutState.mDepthFormat, inOutState.mMsaaCount, inOutState.mMemoryProperties);
	}
}

void NPoVulkanDeviceBehavior::add_window(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings, GLFWwindow *pWindow)
{
	using namespace NPoVulkanDevicePrivate;

	SPoVulkanWindowSettings const &windowSettings = settings.mWindowsSettings;
	SPoVulkanWindowState &windowState = inOutState.mWindowStateVector.emplace_back();
	SPoVulkanWindowResources &windowResources = inOutResources.mWindowResourcesVector.emplace_back();

	windowState.mpWindow = pWindow;
	create_surface(windowResources.mSurface, inOutState.mInstance, pWindow);
	if (inOutResources.mLogicalDevice == VK_NULL_HANDLE)
	{
		inOutState.mPhysicalDevice = pick_physical_device(inOutState.mInstance, windowResources.mSurface, settings);
		inOutState.mMsaaCount = get_max_usable_sample_count(inOutState.mPhysicalDevice);
		inOutState.mQueueFamilyIndices = NPoVulkanDeviceBehavior::find_queue_families(windowResources.mSurface, inOutState.mPhysicalDevice);
		vkGetPhysicalDeviceMemoryProperties(inOutState.mPhysicalDevice, &(inOutState.mMemoryProperties));
	}
	windowState.mSwapchainSupportDetails = NPoVulkanSwapchainBehavior::query_swap_chain_support(windowResources.mSurface, inOutState.mPhysicalDevice);
	if (inOutResources.mLogicalDevice == VK_NULL_HANDLE)
	{
		inOutState.mSurfaceFormat = NPoVulkanSwapchainBehavior::choose_swap_surface_format(windowState.mSwapchainSupportDetails.mFormats);
		inOutState.mDepthFormat = find_depth_format(inOutState.mPhysicalDevice);

		create_logical_device(inOutResources.mLogicalDevice, inOutState, settings, inOutState.mQueueFamilyIndices);
		create_command_pool(inOutResources.mCommandPool, inOutResources.mLogicalDevice, inOutState.mQueueFamilyIndices);
		inOutState.mTexture = NPoTextureBehavior::init(inOutResources.mTexture, settings.mTexture,
			inOutResources.mLogicalDevice, inOutState.mPhysicalDevice, inOutResources.mCommandPool, inOutState.mGraphicsQueue, inOutState.mMemoryProperties);
		inOutState.mMesh = NPoMeshBehavior::init(inOutResources.mMesh, settings.mMesh, inOutResources.mLogicalDevice, inOutState.mPhysicalDevice, inOutResources.mCommandPool,
			inOutState.mGraphicsQueue, inOutState.mMemoryProperties);
		inOutState.mSlang = NPoSlangBehavior::init(inOutResources.mSlang, settings.mSlang, inOutResources.mLogicalDevice);
	}
	SPoVulkanDeviceState const &state = inOutState;
	// TODO : change this to take a window pointer that's it. windows themselves will be 
	glfwSetFramebufferSizeCallback(pWindow, framebufferResizeCallback);

	create_command_pool(windowResources.mCommandPool, inOutResources.mLogicalDevice, state.mQueueFamilyIndices);
	VkExtent2D extent = NPoVulkanSwapchainBehavior::choose_swap_extent(pWindow, windowState.mSwapchainSupportDetails.mCapabilities);
	create_render_pass(windowResources.mRenderPass, inOutResources.mLogicalDevice, state.mMsaaCount, state.mSurfaceFormat.format, state.mDepthFormat);
	create_descriptor_set_layout(windowResources.mDescriptorSetLayout, inOutResources.mLogicalDevice);
	create_graphics_pipeline(windowResources.mPipelineLayout, windowResources.mGraphicsPipeline, inOutResources.mLogicalDevice, windowResources.mDescriptorSetLayout, windowResources.mRenderPass, windowState.mSwapchain.mExtent, state.mMsaaCount, state.mSlang, inOutResources.mSlang);
	windowState.mSwapchain = NPoVulkanSwapchainBehavior::init(windowResources.mSwapchain, windowResources.mSurface, inOutResources.mLogicalDevice, windowResources.mCommandPool, state.mGraphicsQueue, windowResources.mRenderPass,
		windowSettings.mSwapchain, windowState.mSwapchainSupportDetails, state.mQueueFamilyIndices, extent, state.mSurfaceFormat, state.mDepthFormat, state.mMsaaCount, state.mMemoryProperties);
	create_descriptor_pool(windowResources.mDescriptorPool, inOutResources.mLogicalDevice, windowState.mSwapchain.mImageCount * NPoGameObjectBehavior::gk_max_game_objects);
	std::vector<VkDescriptorSetLayout> layouts(windowState.mSwapchain.mImageCount, windowResources.mDescriptorSetLayout);

	// hard coded game objects not a big deal for now we'll figure out the structure later
	// TODO : Figure out the structure
	for (int i = 0; i < windowState.mGameObjects.size(); ++i)
	{
		windowState.mGameObjects[i] = NPoGameObjectBehavior::init(windowResources.mGameObjects[i], settings.mWindowsSettings.mGameObjects[i],
			inOutResources.mLogicalDevice, inOutResources.mTexture.mImageView, inOutResources.mTexture.mSampler,
			layouts, windowResources.mDescriptorPool, state.mMemoryProperties, windowState.mSwapchain.mImageCount);
	}

	create_command_buffers(windowResources.mCommandBuffers, inOutResources.mLogicalDevice, windowResources.mCommandPool, windowState.mSwapchain.mImageCount);

	create_sync_objects(windowResources.mImageAvailableSemaphores, windowResources.mRenderFinishedSemaphores, windowResources.mInFlightFences, inOutResources.mLogicalDevice, windowState.mSwapchain.mImageCount);
}

void NPoVulkanDeviceBehavior::close_window(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings, GLFWwindow *pWindow)
{
	if (pWindow == nullptr)
	{
		return;
	}
	int windowIndex = 0;
	for (;windowIndex < inOutResources.mWindowResourcesVector.size(); ++windowIndex)
	{
		if (inOutState.mWindowStateVector[windowIndex].mpWindow == pWindow)
		{
			break;
		}
	}


	SPoVulkanWindowSettings const &windowSettings = settings.mWindowsSettings;
	SPoVulkanWindowState &windowState = inOutState.mWindowStateVector[windowIndex];
	SPoVulkanWindowResources &windowResources = inOutResources.mWindowResourcesVector[windowIndex];

	int const imageCount = windowState.mSwapchain.mImageCount;
	for (int i = 0; i < windowState.mGameObjects.size(); ++i)
	{
		NPoGameObjectBehavior::cleanup(windowResources.mGameObjects[i], windowState.mGameObjects[i], settings.mWindowsSettings.mGameObjects[i]);
	}

	NPoVulkanSwapchainBehavior::cleanup(windowResources.mSwapchain, windowState.mSwapchain, settings.mWindowsSettings.mSwapchain);

	vkDestroyDescriptorPool(inOutResources.mLogicalDevice, windowResources.mDescriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(inOutResources.mLogicalDevice, windowResources.mDescriptorSetLayout, nullptr);

	vkDestroyPipeline(inOutResources.mLogicalDevice, windowResources.mGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(inOutResources.mLogicalDevice, windowResources.mPipelineLayout, nullptr);
	vkDestroyRenderPass(inOutResources.mLogicalDevice, windowResources.mRenderPass, nullptr);


	for (size_t i = 0; i < imageCount; ++i)
	{
		vkDestroySemaphore(inOutResources.mLogicalDevice, windowResources.mImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(inOutResources.mLogicalDevice, windowResources.mRenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(inOutResources.mLogicalDevice, windowResources.mInFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(inOutResources.mLogicalDevice, windowResources.mCommandPool, nullptr);
	vkDestroySurfaceKHR(inOutState.mInstance, windowResources.mSurface, nullptr);


	size_t endIndex = inOutState.mWindowStateVector.size() - 1;
	inOutState.mWindowStateVector[windowIndex] = std::move(inOutState.mWindowStateVector[endIndex]);
	inOutResources.mWindowResourcesVector[windowIndex] = std::move(inOutResources.mWindowResourcesVector[endIndex]);
	inOutState.mWindowStateVector.pop_back();
	inOutResources.mWindowResourcesVector.pop_back();
}


void NPoVulkanDeviceBehavior::draw_window_frames(SPoVulkanDeviceResources &inOutResources, SPoVulkanDeviceState &inOutState, SPoVulkanDeviceSettings const &settings)
{
	for (int windowIndex = 0; windowIndex < inOutResources.mWindowResourcesVector.size(); ++windowIndex)
	{

		SPoVulkanWindowResources &windowResources = inOutResources.mWindowResourcesVector[windowIndex];
		SPoVulkanWindowState &windowState = inOutState.mWindowStateVector[windowIndex];
		SPoVulkanWindowSettings const &windowSettings = settings.mWindowsSettings;
		using namespace NPoVulkanDevicePrivate;
		int const imageCount = windowState.mSwapchain.mImageCount;
		int &currentFrame = windowState.mCurrentFrame;
		VkDevice device = inOutResources.mLogicalDevice;
		vkWaitForFences(inOutResources.mLogicalDevice, 1, &windowResources.mInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);


		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, windowResources.mSwapchain.mSwapchain, UINT64_MAX, windowResources.mImageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			NPoVulkanDeviceBehavior::recreate_swap_chains_for_resize(inOutResources, inOutState, settings, windowState.mpWindow);
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		vkResetFences(device, 1, &windowResources.mInFlightFences[currentFrame]);

		vkResetCommandBuffer(windowResources.mCommandBuffers[currentFrame], 0);

		record_command_buffer(windowResources.mCommandBuffers[currentFrame], imageIndex, currentFrame,
			windowResources.mRenderPass, windowResources.mSwapchain.mFrameBuffers, windowState.mSwapchain.mExtent,
			windowResources.mGraphicsPipeline, windowResources.mPipelineLayout, inOutResources.mMesh,
			windowResources.mGameObjects, windowState.mGameObjects);


		glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		glm::mat4 projection = glm::perspective(glm::radians(45.0f), windowState.mSwapchain.mExtent.width / (float)windowState.mSwapchain.mExtent.height, 0.1f, 10.0f);
		projection[1][1] *= -1.0;
		for (int i = 0; i < NPoGameObjectBehavior::gk_max_game_objects; ++i)
		{
			NPoGameObjectBehavior::update_uniform_buffers(windowResources.mGameObjects[i], windowState.mGameObjects[i], settings.mWindowsSettings.mGameObjects[i], currentFrame, view, projection);
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { windowResources.mImageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &windowResources.mCommandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { windowResources.mRenderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(inOutState.mGraphicsQueue, 1, &submitInfo, windowResources.mInFlightFences[currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { windowResources.mSwapchain.mSwapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		result = vkQueuePresentKHR(inOutState.mPresentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowState.mFramebufferResized)
		{
			windowState.mFramebufferResized = false;
			NPoVulkanDeviceBehavior::recreate_swap_chains_for_resize(inOutResources, inOutState, settings, windowState.mpWindow);
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}

		windowState.mCurrentFrame = (windowState.mCurrentFrame + 1) % imageCount;
	}
}