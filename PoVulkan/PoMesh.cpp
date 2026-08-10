#include "PoMesh.h"
#include "PoVulkanDevice.h"


#include <vector>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

// TODO : I am thinking I might want to rename these as PoVulkanMeshses since they are very specific to vulkan implementation

namespace std
{
	template<> struct hash<Vertex>;
}

namespace NPoMeshPrivate
{

	void load_model(std::vector<Vertex> &outVertices, std::vector<uint32_t> &outIndices, SPoMeshSettings const &settings)
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		std::string warning;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, &warning, settings.mPath.c_str()))
		{
			throw std::runtime_error(err);
		}
		std::unordered_map<Vertex, uint32_t> uniqueVertices{};


		for (const auto &shape : shapes)
		{
			for (const auto &index : shape.mesh.indices)
			{
				Vertex vertex{};

				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0 - attrib.texcoords[2 * index.texcoord_index + 1]
				};
				vertex.color = { 1.0f, 1.0f, 1.0f };

				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(outVertices.size());
					outVertices.push_back(vertex);
				}
				outIndices.push_back(uniqueVertices[vertex]);
			}
		}
	}
}

SPoMeshState NPoMeshBehavior::init(SPoMeshResources &outResources, SPoMeshSettings const settings,
	VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
	VkPhysicalDeviceMemoryProperties const &memoryProperties)
{
    SPoMeshState state = {};
	state.mDevice = device;
	NPoMeshPrivate::load_model(outResources.mVertices, outResources.mIndices, settings);

	VkDeviceSize vertexBufferSize = sizeof(outResources.mVertices[0]) * outResources.mVertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	NPoVulkanDeviceBehavior::create_buffer(stagingBuffer, stagingBufferMemory,
		device, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		memoryProperties);

	void *data;
	vkMapMemory(device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
	memcpy(data, outResources.mVertices.data(), (size_t)vertexBufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	NPoVulkanDeviceBehavior::create_buffer(outResources.mVertexBuffer, outResources.mVertexBufferMemory,
		device, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryProperties);

	NPoVulkanDeviceBehavior::copy_buffer(outResources.mVertexBuffer, stagingBuffer, device, commandPool, graphicsQueue, vertexBufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);

	VkDeviceSize indexBufferSize = sizeof(outResources.mIndices[0]) * outResources.mIndices.size();

	NPoVulkanDeviceBehavior::create_buffer(stagingBuffer, stagingBufferMemory,
		device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		memoryProperties);

	vkMapMemory(device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
	memcpy(data, outResources.mIndices.data(), (size_t)indexBufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	NPoVulkanDeviceBehavior::create_buffer(outResources.mIndexBuffer, outResources.mIndexBufferMemory,
		device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryProperties);

	NPoVulkanDeviceBehavior::copy_buffer(outResources.mIndexBuffer, stagingBuffer, device, commandPool, graphicsQueue, indexBufferSize);
	
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
    return state;
}

void NPoMeshBehavior::cleanup(SPoMeshResources &inOutResources, SPoMeshState &inOutState, SPoMeshSettings const &settings)
{
	vkDestroyBuffer(inOutState.mDevice, inOutResources.mVertexBuffer, nullptr);
	vkFreeMemory(inOutState.mDevice, inOutResources.mVertexBufferMemory, nullptr);

	vkDestroyBuffer(inOutState.mDevice, inOutResources.mIndexBuffer, nullptr);
	vkFreeMemory(inOutState.mDevice, inOutResources.mIndexBufferMemory, nullptr);

    inOutResources = {};
    inOutState = {};
}