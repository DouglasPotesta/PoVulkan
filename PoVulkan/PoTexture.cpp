#include "PoTexture.h"
#include "PoVulkanDevice.h"

#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


VkSampler NPoTextureBehavior::create_texture_sampler(VkDevice device, VkPhysicalDevice physicalDevice)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    VkSampler sampler;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create sampler!");
    }
    return sampler;
}


SPoTextureState NPoTextureBehavior::init(SPoTextureResources &outResources, SPoTextureSettings const &settings, 
    VkDevice device, VkPhysicalDevice physicalDevice, VkBuffer stagingBuffer, VkDeviceMemory stagingBufferMemory, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBufferUsageFlags const usage, VkMemoryPropertyFlags const properties, VkPhysicalDeviceMemoryProperties const &memoryProperties)
{
    SPoTextureState state;
    state.mDevice = device;

    stbi_uc *pPixels = stbi_load(settings.mPath.c_str(), &state.mWidth, &state.mHeight, &state.mChannels, STBI_rgb_alpha);
    state.mImageSize = state.mHeight * state.mHeight * 4;

    if (pPixels == nullptr)
    {
        throw std::runtime_error("failed to load texture image!");
    }

    switch (settings.mMipMapCreationType)
    {
    case EPoMipmapCreationType::None:
        state.mMipLevels = 0;
        break;
    case EPoMipmapCreationType::Max:
        state.mMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(state.mWidth, state.mHeight)))) + 1;
        break;
    default:
        throw std::runtime_error("The requested mip map creation type is not implemented!");
        break;
    }
    static VkFormat const skImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
    
    NPoVulkanDeviceBehavior::create_buffer(stagingBuffer, stagingBufferMemory, 
        device, state.mImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        memoryProperties);

    void *data;
    vkMapMemory(device, stagingBufferMemory, 0, state.mImageSize, 0, &data);
    memcpy(data, pPixels, static_cast<size_t>(state.mImageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pPixels);
    NPoVulkanDeviceBehavior::create_image(outResources.mImage, outResources.mImageMemory, device, state.mWidth, state.mHeight, state.mMipLevels, VK_SAMPLE_COUNT_1_BIT,
        skImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryProperties);
    NPoVulkanDeviceBehavior::transition_image_layout(device, commandPool, graphicsQueue, outResources.mImage, skImageFormat, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, state.mMipLevels);

    NPoVulkanDeviceBehavior::copy_buffer_to_image(outResources.mImage, stagingBuffer, device, commandPool, graphicsQueue, static_cast<uint32_t>(state.mWidth), static_cast<uint32_t>(state.mHeight));
        //transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
        //	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);

    NPoVulkanDeviceBehavior::generate_mipmaps(outResources.mImage, device, physicalDevice, commandPool, graphicsQueue,
        skImageFormat, state.mWidth, state.mHeight, state.mMipLevels);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    outResources.mImageView = NPoVulkanDeviceBehavior::create_image_view(device, outResources.mImage, skImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, state.mMipLevels);
    outResources.mSampler = create_texture_sampler(device, physicalDevice);
    return state;
}

void NPoTextureBehavior::cleanup(SPoTextureResources &inOutResources, SPoTextureState &inOutState, SPoTextureSettings const &settings)
{
    vkDestroySampler(inOutState.mDevice, inOutResources.mSampler, nullptr);
    vkDestroyImageView(inOutState.mDevice, inOutResources.mImageView, nullptr);
    vkDestroyImage(inOutState.mDevice, inOutResources.mImage, nullptr);
    vkFreeMemory(inOutState.mDevice, inOutResources.mImageMemory, nullptr);
    inOutResources = {};
    inOutState = {};
}
