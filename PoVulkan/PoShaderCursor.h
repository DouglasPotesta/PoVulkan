#pragma once

#include "PoSlangPipelineLayoutBuilder.h"
#include <array>
#include <cstring>

namespace NPoShaderCursor
{
	using namespace NPoPipelineLayoutBuilder;
	struct STextureView { VkImageView mView; VkImageLayout mLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; };
	struct SSampler { VkSampler mHandle; };
	struct SBufferView { VkBuffer mBuffer; VkDeviceSize mOffset; VkDeviceSize mSize; };

	struct SShaderOffset
	{
		size_t mBytes = 0;
		uint32_t mRange = 0;
		uint32_t mArrayElement = 0;
	};

	class SShaderCursor
	{
	public:
		std::shared_ptr<SShaderObject> mpObject;
		slang::TypeLayoutReflection *mpType;
		SShaderOffset mOffset = {};

		explicit SShaderCursor(std::shared_ptr<SShaderObject> value)
			: mpObject(std::move(value)), mpType(mpObject ? mpObject->mpType : nullptr)
		{
			if (mpType == nullptr) throw std::runtime_error("Null cursor object");
		}

		SShaderCursor Dereference() const
		{
			auto result = *this;
			while (result.mpType->getKind() == slang::TypeReflection::Kind::ParameterBlock ||
				result.mpType->getKind() == slang::TypeReflection::Kind::ConstantBuffer)
			{
				if (result.mOffset.mArrayElement != 0) throw std::runtime_error("Block arrays unsupported");
				auto pChild = result.mpObject->mChildren[result.mOffset.mRange];
				if (pChild == nullptr) throw std::runtime_error("Missing block Object");
				result = SShaderCursor(pChild);
			}
			return result;
		}

		SShaderCursor Field(uint32_t index) const
		{
			auto result = Dereference();
			if (result.mpType->getKind() != slang::TypeReflection::Kind::Struct || index >= result.mpType->getFieldCount())
				throw std::runtime_error("Invalid field index");
			auto field = result.mpType->getFieldByIndex(index);
			result.mOffset.mBytes += field->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM);
			result.mOffset.mRange += checked_count(result.mpType->getFieldBindingRangeOffset(index));
			result.mpType = field->getTypeLayout();
			return result;
		}

		SShaderCursor Field(const char *name) const
		{
			auto parent = Dereference();
			if (parent.mpType->getKind() != slang::TypeReflection::Kind::Struct)
				throw std::runtime_error("field() requires a struct");
			auto index = parent.mpType->findFieldIndexByName(name);
			if (index < 0) throw std::runtime_error(std::string("Missing shader field: ") + name);
			return parent.Field(static_cast<uint32_t>(index));
		}

		SShaderCursor Element(uint32_t index) const
		{
			auto result = Dereference();
			if (result.mpType->getKind() != slang::TypeReflection::Kind::Array)
				throw std::runtime_error("Element() requires an array");
			auto const count = checked_count(result.mpType->getElementCount());
			if (index >= count) throw std::runtime_error("Array index out of bounds");
			auto const stride = result.mpType->getElementStride(SLANG_PARAMETER_CATEGORY_UNIFORM);
			result.mOffset.mBytes += size_t(index) * stride;
			result.mOffset.mArrayElement = checked_count(size_t(result.mOffset.mArrayElement) * count + index);
			result.mpType = result.mpType->getElementTypeLayout();
			return result;
		}

		void WriteOrdinary(void const *source, size_t size, slang::TypeReflection::ScalarType scalar, uint32_t count) const
		{
			auto const expectedKind = count == 1 ? slang::TypeReflection::Kind::Scalar : slang::TypeReflection::Kind::Vector;
			if (mpType->getKind() != expectedKind || mpType->getType()->getScalarType() != scalar ||
				(count != 1 && mpType->getElementCount() != count) || mpType->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM) != size)
				throw std::runtime_error("Ordinary value type/size mismatch");
			if (mOffset.mBytes > mpObject->mBytes.size() || size > mpObject->mBytes.size() - mOffset.mBytes)
				throw std::runtime_error("Uniform write exceeds object storage");
			std::memcpy(mpObject->mBytes.data() + mOffset.mBytes, source, size);
		}

		void Write(float value) const { WriteOrdinary(&value, sizeof(value), slang::TypeReflection::ScalarType::Float32, 1); }
		void Write(uint32_t value) const { WriteOrdinary(&value, sizeof(value), slang::TypeReflection::ScalarType::UInt32, 1); }
		void Write(std::array<float, 4> const &value) const
		{
			WriteOrdinary(value.data(), sizeof(float) * value.size(), slang::TypeReflection::ScalarType::Float32, 4);
		}
		// Explicit Spir-v handle encoding; reflectionchecks uint2 not resource semantics.
		void WriteHandle(uint32_t index, uint32_t reserved = 0) const
		{
			uint32_t const words[2] = { index, reserved };
			WriteOrdinary(words, sizeof(words), slang::TypeReflection::ScalarType::UInt32, 2);
		}

		SResourceRange &ResourceRange() const
		{
			auto const kind = mpType->getKind();
			if (kind != slang::TypeReflection::Kind::Resource && kind != slang::TypeReflection::Kind::SamplerState &&
				kind != slang::TypeReflection::Kind::ShaderStorageBuffer)
				throw std::runtime_error("Resource write requires a resource leaf");
			auto &range = mpObject->mRanges[mOffset.mRange];
			if (!range) throw std::runtime_error("No resource binding at cursor");
			return *range;
		}

		void Write(STextureView value) const
		{
			auto &range = ResourceRange();
			if (range.mType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && range.mType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				throw std::runtime_error("Texture write to non-texture field");
			auto &target = range.mElements[mOffset.mArrayElement];
			target.mImage = { VK_NULL_HANDLE, value.mView, value.mLayout };
			target.mAssigned = true;
		}
		void Write(SSampler value) const
		{
			auto &range = ResourceRange();
			if (range.mType != VK_DESCRIPTOR_TYPE_SAMPLER) throw std::runtime_error("Sampler type mismatch");
			auto &target = range.mElements[mOffset.mArrayElement];
			target.mImage = { value.mHandle, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };
			target.mAssigned = true;
		}
		void Write(SBufferView value) const
		{
			auto &range = ResourceRange();
			if (range.mType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) throw std::runtime_error("Storage-buffer type mismatch");
			if (value.mSize == 0) throw std::runtime_error("Zero buffer view size");
			auto &target = range.mElements[mOffset.mArrayElement];
			target.mBuffer = { value.mBuffer, value.mOffset, value.mSize };
			target.mAssigned = true;
		}
	};

	struct SMaterialData 
	{
		std::array<float, 4> mTint = { 1, 2, 3, 4 };
		std::array<STextureView, 4> mLayers = {};
		SSampler mSampler = {};
		void WriteInto(SShaderCursor cursor) const
		{
			cursor.Field("tint").Write(mTint);
			for (uint32_t i = 0; i < mLayers.size(); ++i)
			{
				cursor.Field("layers").Element(i).Write(mLayers[i]);
			}
			cursor.Field("sampler").Write(mSampler);
		}
	};

	struct SSceneData
	{
		std::array<float, 4> mAmbient{ 0.3f, 0.2f, 0.5f, 0.4f };
		SMaterialData mMaterial;
		void WriteInto(SShaderCursor cursor) const
		{
			cursor.Field("ambient").Write(mAmbient);
			mMaterial.WriteInto(cursor.Field("material"));
		}
	};

	inline void WriteParameters(SProgramParameters const &parameters, SSceneData const &scene, SBufferView output, uint32_t count)
	{
		scene.WriteInto(SShaderCursor(parameters.mGlobals).Field("scene"));
		SShaderCursor entry(parameters.mEntryPoints[0]);
		entry.Field("output").Write(output);
		entry.Field("count").Write(count);
	}
}