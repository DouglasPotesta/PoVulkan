#include "PoGui.h"
#include "PoVulkanDevice.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <stdexcept>

namespace NPoGuiPrivate
{
    void create_descriptor_pool(VkDescriptorPool &outDescriptorPool, VkDevice device)
    {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
        };

        VkDescriptorPoolCreateInfo poolCreateInfo = VkDescriptorPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1000 * IM_ARRAYSIZE(poolSizes),
            .poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes),
            .pPoolSizes = poolSizes
        };

        if (vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &outDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create imgui's descriptor pool!");
        }
    }

    void begin_rendering(VkCommandBuffer commandBuffer, VkImage swapChainResolveImage, VkImage swapChainColorImage, VkImageView swapChainResolveImageView, VkImageView swapChainColorImageView, VkImageView swapChainDepthImageView, VkExtent2D const extent, VkFormat const colorFormat, VkClearValue const *pColorClear, VkClearValue const *pDepthClear)
    {
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderingAttachmentInfo colorAttachmentInfo = {};
        colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        colorAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.resolveImageView = swapChainResolveImageView;
        colorAttachmentInfo.imageView = swapChainColorImageView;
        colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp = pColorClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        if (pColorClear != nullptr)
        {
            colorAttachmentInfo.clearValue = *pColorClear;
        }

        VkRenderingAttachmentInfoKHR depthAttachmentInfo = {};
        depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
        depthAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
        depthAttachmentInfo.imageView = swapChainDepthImageView;
        depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachmentInfo.loadOp = pDepthClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        if (pDepthClear != nullptr)
        {
            depthAttachmentInfo.clearValue = *pDepthClear;
        }


        VkRect2D renderArea = VkRect2D{ VkOffset2D{}, extent };
        VkRenderingInfo renderInfo = {};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.flags = 0;
        renderInfo.renderArea = renderArea;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachmentInfo;
        renderInfo.pDepthAttachment = &depthAttachmentInfo;
        renderInfo.pStencilAttachment = VK_NULL_HANDLE;

        vkCmdBeginRendering(commandBuffer, &renderInfo);
    }
}

SPoGuiState NPoGuiBehavior::init(SPoGuiResources &outResources, SPoGuiSettings const &settings, VkDevice device, int const width, int const height, GLFWwindow *pWindow, VkFormat const colorFormat,
    uint32_t const apiVersion, VkInstance instance, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, uint32_t const queueFamily, VkQueue queue, uint32_t const minImageCount, uint32_t const imageCount, VkSampleCountFlagBits const msaaSamples,
    VkPipelineCache pipelineCache, VkFormat const depthFormat)
{
    using namespace NPoGuiPrivate;
    SPoGuiState state = {};
    state.mDevice = device;
    state.mCommandPool = commandPool;
    create_descriptor_pool(outResources.mDescriptorPool, device);
    state.mImageCount = imageCount;

    outResources.mpContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(outResources.mpContext);
    
    ImGuiIO &io = ImGui::GetIO();
    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.DisplaySize.x = (float)width;
    io.DisplaySize.y = (float)height;

    ImGui::GetStyle().FontScaleMain = 1.5f;

    ImGui::StyleColorsDark();

    bool installGlfwCallbacks = true;

    static bool sIsInitialized = false;

    // TODO : Produce a proper application initialization scheme for this.
    //if (!sIsInitialized)
    {
        sIsInitialized = true;
        ImGui_ImplGlfw_InitForVulkan(pWindow, installGlfwCallbacks);

    }
    ImGui_ImplVulkan_InitInfo initInfo = ImGui_ImplVulkan_InitInfo{
        .ApiVersion = apiVersion,
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,
        .QueueFamily = queueFamily,
        .Queue = queue,
        .DescriptorPool = outResources.mDescriptorPool,
        .MinImageCount = minImageCount,
        .ImageCount = imageCount,
        .PipelineCache = pipelineCache,
        .PipelineInfoMain = ImGui_ImplVulkan_PipelineInfo{
            .RenderPass = VK_NULL_HANDLE,
            .Subpass = 0,
            .MSAASamples = msaaSamples,
            .ExtraDynamicStates = {},
            .PipelineRenderingCreateInfo = VkPipelineRenderingCreateInfoKHR{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .pNext = nullptr,
                .viewMask = 0,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &colorFormat,
                .depthAttachmentFormat = depthFormat,
                .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
            },
        },
        .UseDynamicRendering = true,
        .Allocator = VK_NULL_HANDLE,
        .CheckVkResultFn = [](VkResult result) { if (result != VK_SUCCESS) throw std::runtime_error("failed to create imgui!"); },
    };

    ImGui_ImplVulkan_Init(&initInfo);

    outResources.mCommandBuffers.resize(imageCount);

    NPoVulkanDeviceBehavior::create_command_buffers(outResources.mCommandBuffers, device, commandPool, imageCount);

    return state;
}

VkCommandBuffer NPoGuiBehavior::draw(SPoGuiResources &resources, VkImage swapChainResolveImage, VkImage swapChainColorImage, VkImageView swapChainResolveImageView, VkImageView swapChainColorImageView, VkImageView swapChainDepthImageView, VkExtent2D const extent, VkFormat const colorFormat, int const imageIndex)
{
    ImGui::SetCurrentContext(resources.mpContext);
    using namespace NPoGuiPrivate;
    VkCommandBuffer buffer = resources.mCommandBuffers[imageIndex];
    NPoVulkanDeviceBehavior::begin_command_buffer(buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    begin_rendering(buffer, swapChainResolveImage, swapChainColorImage, swapChainResolveImageView, swapChainColorImageView, swapChainDepthImageView, extent, colorFormat, /*pColorClear*/ nullptr, /*pDepthClear*/ nullptr);

    if (ImDrawData *pDrawData = ImGui::GetDrawData())
    {
        ImGui_ImplVulkan_RenderDrawData(pDrawData, buffer);
    }
    vkCmdEndRendering(buffer);

    NPoVulkanDeviceBehavior::transition_image_layout_command(buffer, swapChainResolveImage, colorFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1);

    vkEndCommandBuffer(buffer);
    return buffer;
}

void NPoGuiBehavior::cleanup(SPoGuiResources &inOutResources, SPoGuiState &inOutState, SPoGuiSettings const &settings)
{

    vkFreeCommandBuffers(inOutState.mDevice, inOutState.mCommandPool, static_cast<uint32_t>(inOutResources.mCommandBuffers.size()), inOutResources.mCommandBuffers.data());

    ImGui::SetCurrentContext(inOutResources.mpContext);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    
    ImGui::DestroyContext(inOutResources.mpContext);

    vkDestroyDescriptorPool(inOutState.mDevice, inOutResources.mDescriptorPool, nullptr);

    inOutResources = {};
    inOutState = {};
}