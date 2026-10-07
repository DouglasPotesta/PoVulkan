#pragma once

#include "PoShaderCursor.h"

namespace NPoBindlessResources
{
	using namespace NPoShaderCursor;
	using namespace NPoPipelineLayoutBuilder;

	struct SDescriptorBits
	{
		uint32_t mIndex = 0;
		uint32_t mReserved = 0;
	};
	enum class EHandleKind { Texture, Sampler };

	struct SResourceHandle
	{
		uint32_t mSlot = 0;
		uint64_t mGeneration = 0;
		EHandleKind mKind = EHandleKind::Texture;
		void const *mpOwner = nullptr;
	};

	class CHandleTable
	{
	public:
		struct SSlot
		{
			uint64_t mGeneration = 1;
			bool mLive = false;
			VkDescriptorImageInfo mDescriptor = {};
			std::shared_ptr<void> mpResource;
		};
		// TODO : make this an indexable array of vectors or something so that we can easily add other handle kinds
		std::vector<SSlot> mTextures;
		std::vector<SSlot> mSamplers;
		explicit CHandleTable(uint32_t capacity) : mTextures(capacity), mSamplers(capacity)
		{
			if (capacity == 0) throw std::runtime_error("Empty bindless table");
		}
		CHandleTable(CHandleTable const &) = delete;
		CHandleTable &operator=(CHandleTable const &) = delete;
		SResourceHandle Insert(EHandleKind kind, VkDescriptorImageInfo descriptor, std::shared_ptr<void> resource)
		{
			if (!resource) throw std::runtime_error("Resource lifetime owner required");
			auto &slots = Table(kind);
			for (uint32_t i = 0; i < slots.size(); ++i)
			{
				if (!(slots[i].mLive))
				{
					auto &slot = slots[i];
					slot.mLive = true;
					slot.mDescriptor = descriptor;
					slot.mpResource = std::move(resource);
					SResourceHandle handle = { 
						.mSlot = i, 
						.mGeneration = slot.mGeneration, 
						.mKind = kind, 
						.mpOwner = this };
					return handle;
				}
			}
			throw std::runtime_error("Bindless handle table full.");
		}
		SDescriptorBits Bits(SResourceHandle h, EHandleKind expected) const
		{
			Validate(h, expected);
			SDescriptorBits result = {
				.mIndex = h.mSlot,
				.mReserved = 0
			};
			return result;
		}
		void Release(SResourceHandle h)
		{
			Validate(h, h.mKind);
			auto &slot = Table(h.mKind)[h.mSlot];
			if (slot.mGeneration = UINT64_MAX) throw std::runtime_error("Handle generation exhausted");
			slot.mLive = false;
			++(slot.mGeneration);
			slot.mpResource.reset();
			slot.mDescriptor = {};
		}
	private:
		void Validate(SResourceHandle h, EHandleKind expected) const
		{
			auto const &slots = const_cast<CHandleTable *>(this)->Table(expected);
			if (h.mpOwner != this ||
				h.mKind != expected ||
				slots[h.mSlot].mGeneration != h.mGeneration ||
				h.mSlot >= slots.size() ||
				!(slots[h.mSlot].mLive))
				throw std::runtime_error("Invalid resource handle");
		}
		std::vector<SSlot> &Table(EHandleKind kind)
		{
			switch (kind)
			{
			case EHandleKind::Texture:
				return mTextures;
			case EHandleKind::Sampler:
				return mSamplers;
			default:
				throw std::runtime_error("Handle Kind not implemented");
			}
		}
	};

	class CBindlessResources 
	{
	public:
		static constexpr uint32_t skCapacity = 16;
		struct SSnapshot
		{
			VkDescriptorPool mPool = VK_NULL_HANDLE;
			VkDescriptorSet mSet = VK_NULL_HANDLE;
			uint64_t mSerial = 0;
			uint64_t mRevision = 0;
			std::vector<std::shared_ptr<void>> mRetained;
		};
		VkDevice mDevice;
		VkDescriptorSetLayout mLayout;
		std::vector<SSnapshot> mSnapshots;
		SResourceHandle mFallbackTexture;
		SResourceHandle mFallbackSampler;
		CBindlessResources(VkDevice device, VkDescriptorSetLayout layout, 
			STextureView texture, SSampler sampler, std::shared_ptr<void> pResourceOwner) :
			mDevice(device), mLayout(layout)
		{
			mFallbackTexture = RegisterTexture(texture, pResourceOwner);
			mFallbackSampler= RegisterSampler(sampler, pResourceOwner);
		}
		CBindlessResources(CBindlessResources const &) = delete;
		CBindlessResources &operator=(CBindlessResources const &) = delete;
		~CBindlessResources()
		{
			for (auto &slot : mSnapshots)
			{
				vkDestroyDescriptorPool(mDevice, slot.mPool, nullptr);
			}
		}
		SResourceHandle RegisterTexture(STextureView view, std::shared_ptr<void> pOwner)
		{
			if (view.mView == VK_NULL_HANDLE) throw std::runtime_error("Null texture");
			auto handle = mHandles.Insert(EHandleKind::Texture, { VK_NULL_HANDLE, view.mView, view.mLayout }, std::move(pOwner));
			++mRevision;
			return handle;
		}
		SResourceHandle RegisterSampler(SSampler sampler, std::shared_ptr<void> pOwner)
		{
			if (sampler.mHandle == VK_NULL_HANDLE) throw std::runtime_error("Null sampler");
			auto handle = mHandles.Insert(EHandleKind::Sampler, { sampler.mHandle, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED }, std::move(pOwner));
			++mRevision;
			return handle;
		}
		void Release(SResourceHandle handle)
		{
			if ((handle.mKind == EHandleKind::Texture && handle.mSlot == mFallbackTexture.mSlot) ||
				(handle.mKind == EHandleKind::Sampler && handle.mSlot == mFallbackSampler.mSlot))
				throw std::runtime_error("Cannot release fallback");
			mHandles.Release(handle); 
			++mRevision;
		}
		void WriteTexture(SShaderCursor cursor, SResourceHandle handle) const
		{
			auto bits = mHandles.Bits(handle, EHandleKind::Texture); 
			cursor.WriteHandle(bits.mIndex, bits.mReserved);
		}
		void WriteSampler(SShaderCursor cursor, SResourceHandle handle) const
		{
			auto bits = mHandles.Bits(handle, EHandleKind::Sampler);
			cursor.WriteHandle(bits.mIndex, bits.mReserved);
		}

		VkDescriptorSet Snapshot(uint64_t serial)
		{
			if (serial <= mLastSerial) throw std::runtime_error("submission serial must increase");
			if (!mSnapshots.empty() && mSnapshots.back().mRevision == mRevision)
			{
				mSnapshots.back().mSerial = serial;
				mLastSerial = serial;
				return mSnapshots.back().mSet;
			}
			SSnapshot snap;
			snap.mSerial = serial;
			snap.mRevision = mRevision;
			try
			{
				VkDescriptorPoolSize sizes[] = { {VK_DESCRIPTOR_TYPE_SAMPLER, skCapacity}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, skCapacity} };
				VkDescriptorPoolCreateInfo pool = {};
				pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
				pool.maxSets = 1;
				pool.poolSizeCount = 2;
				pool.pPoolSizes = sizes;
				check_vk(vkCreateDescriptorPool(mDevice, &pool, nullptr, &snap.mPool), "bindless pool");
				VkDescriptorSetAllocateInfo setAllocation = {};
				setAllocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				setAllocation.descriptorPool = snap.mPool;
				setAllocation.descriptorSetCount = 1;
				setAllocation.pSetLayouts = &mLayout;
				check_vk(vkAllocateDescriptorSets(mDevice, &setAllocation, &snap.mSet), "bindless set");
				std::array<VkDescriptorImageInfo, skCapacity> textures;
				std::array<VkDescriptorImageInfo, skCapacity> samplers;
				for (uint32_t i = 0; i < skCapacity; ++i)
				{
					auto const &texture = mHandles.mTextures[i].mLive ? mHandles.mTextures[i] : mHandles.mTextures[mFallbackTexture.mSlot];
					auto const &sampler = mHandles.mSamplers[i].mLive ? mHandles.mSamplers[i] : mHandles.mSamplers[mFallbackSampler.mSlot];
					textures[i] = texture.mDescriptor;
					samplers[i] = sampler.mDescriptor;
					snap.mRetained.push_back(texture.mpResource);
					snap.mRetained.push_back(sampler.mpResource);
				}
				VkWriteDescriptorSet writes[2]{};
				for (auto &write : writes)
				{
					write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write.dstSet = snap.mSet;
					write.descriptorCount = skCapacity;
				}
				writes[0].dstBinding = 0;
				writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				writes[0].pImageInfo = samplers.data();
				writes[1].dstBinding = 2; 
				writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				writes[1].pImageInfo = textures.data();
				vkUpdateDescriptorSets(mDevice, 2, writes, 0, nullptr);
				mSnapshots.push_back(std::move(snap));
				mLastSerial = serial;
				return mSnapshots.back().mSet;
			}
			catch (...)
			{
				if (snap.mPool) vkDestroyDescriptorPool(mDevice, snap.mPool, nullptr); throw;
			}
		}

		void Collect(uint64_t completedSerial)
		{
			auto it = mSnapshots.begin();
			while (it != mSnapshots.end() && it + 1 != mSnapshots.end())
			{
				if (it->mSerial <= completedSerial)
				{
					vkDestroyDescriptorPool(mDevice, it->mPool, nullptr);
					it = mSnapshots.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	private:
		CHandleTable mHandles = CHandleTable{ skCapacity };
		uint64_t mLastSerial = 0;
		uint64_t mRevision = 0;
	};

	inline SLayoutDescription WithBindlessSet(SLayoutDescription description, uint32_t set)
	{
		if (set != description.mSets.size()) throw std::runtime_error("Expected appended bindless set");
		description.mSets.push_back({
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = CBindlessResources::skCapacity,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = nullptr},
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = CBindlessResources::skCapacity,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = nullptr}
			});
		return description;
	}

	struct SBindlessMaterial
	{
		std::array<float, 4> mTint{ 3,1,2,4 };
		std::array<SResourceHandle, 4> mLayers;
		SResourceHandle mSampler;
		void WriteInto(SShaderCursor cursor, CBindlessResources const &resources) const
		{
			cursor.Field("tint").Write(mTint);
			for (uint32_t i = 0; i < mLayers.size(); ++i)
			{
				resources.WriteTexture(cursor.Field("layers").Element(i), mLayers[i]);
			}
			resources.WriteSampler(cursor.Field("sampler"), mSampler);
		}
	};

	inline std::vector<std::byte> EncodeMaterials(slang::TypeLayoutReflection *pBufferLayout,
		std::vector<SBindlessMaterial> const &materials, CBindlessResources const &resources)
	{
		auto pType = pBufferLayout->getElementTypeLayout();
		if (pType == nullptr) throw std::runtime_error("Missing structured buffer element layout");
		auto const stride = pType->getStride(SLANG_PARAMETER_CATEGORY_UNIFORM);
		if (stride == 0 || materials.size() > SIZE_MAX / stride) throw std::runtime_error("Invalid material buffer size");
		std::vector<std::byte> bytes = std::vector<std::byte>(stride * materials.size());
		for (size_t i = 0; i < materials.size(); ++i)
		{
			auto pObject = std::make_shared<SShaderObject>(pType);
			pObject->mBytes.resize(stride);
			materials[i].WriteInto(SShaderCursor(pObject), resources);
			std::memcpy(bytes.data() + i * stride, pObject->mBytes.data(), stride);
		}
		return bytes;
	}
}