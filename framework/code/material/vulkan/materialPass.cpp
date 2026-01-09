//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "material.hpp"
#include "shader.hpp"
#include "vulkan/vulkan.hpp"
#include <array>
#include "system/os_common.h"
#include "vulkan/TextureFuncts.h"
#include "vulkanRT/accelerationStructure.hpp"
#include "texture/vulkan/texture.hpp"

static constexpr VkDescriptorType cDescriptorTypeToVk[] {
    VK_DESCRIPTOR_TYPE_MAX_ENUM,                    // unused
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,              // UniformBuffer
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,              // StorageBuffer
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,      // ImageSampler
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,               // ImageSampled
    VK_DESCRIPTOR_TYPE_SAMPLER,                     // Sampler
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,               // ImageStorage,
    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,            // InputAttachment,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,              // DrawIndirectBuffer ///TODO: is this the correct conversion (should we have this descriptor type?)
    VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,  // AccelerationStructure
    VK_DESCRIPTOR_TYPE_MAX_ENUM,                    // descriptor table (not supported by Vk)
};
static_assert(cDescriptorTypeToVk[(int)DescriptorSetDescription::DescriptorType::AccelerationStructure] == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
static_assert(cDescriptorTypeToVk[(int)DescriptorSetDescription::DescriptorType::Sampler] == VK_DESCRIPTOR_TYPE_SAMPLER);

static constexpr VkDescriptorType EnumToVk(const DescriptorSetDescription::DescriptorType t) {
    return cDescriptorTypeToVk[(int)t];
}



MaterialPass<Vulkan>::MaterialPass(Vulkan& vulkan, const ShaderPass<Vulkan>& shaderPass, VkDescriptorPool descriptorPool, std::vector<VkDescriptorSet> descriptorSets, std::vector<VkDescriptorSetLayout> dynamicDescriptorSetLayouts, tTextureBindings textureBindings, tImageBindings imageBindings, tBufferBindings bufferBindings, tAccelerationStructureBindings accelerationStructureBindings, SpecializationConstants<Vulkan> specializationConstants ) noexcept
	: MaterialPassBase(shaderPass)
    , mVulkan( vulkan )
    , mNumDescriptorSetsPerBuffer(uint32_t(shaderPass.GetDescriptorSetLayouts().size()))
    , mNumBuffers(mNumDescriptorSetsPerBuffer>0 ? uint32_t(descriptorSets.size() / mNumDescriptorSetsPerBuffer) : 0)
    , mDescriptorPool(descriptorPool)
	, mDescriptorSets(std::move(descriptorSets))
	, mDynamicDescriptorSetLayouts(std::move(dynamicDescriptorSetLayouts))
    , mSpecializationConstants( std::move( specializationConstants ) )
    , mTextureBindings(std::move(textureBindings))
	, mImageBindings(std::move(imageBindings))
	, mBufferBindings(std::move(bufferBindings))
    , mAccelerationStructureBindings( std::move( accelerationStructureBindings ) )
{
	if (!mDynamicDescriptorSetLayouts.empty())
		mDynamicPipelineLayout.Init(vulkan, mDynamicDescriptorSetLayouts);
    assert( mDescriptorSets.size() == mNumBuffers*mNumDescriptorSetsPerBuffer );

	descriptorPool = VK_NULL_HANDLE;	// we took owenership
}

MaterialPass<Vulkan>::MaterialPass(MaterialPass<Vulkan>&& other) noexcept
	: MaterialPassBase(std::move(other))
    , mVulkan( other.mVulkan )
    , mNumDescriptorSetsPerBuffer( other.mNumDescriptorSetsPerBuffer )
    , mNumBuffers( other.mNumBuffers )
    , mDescriptorPool(other.mDescriptorPool)
	, mDescriptorSets(std::move(other.mDescriptorSets))
	, mDynamicDescriptorSetLayouts(std::move(other.mDynamicDescriptorSetLayouts))
	, mDynamicPipelineLayout(std::move(other.mDynamicPipelineLayout))
    , mAccelerationStructureBindings( std::move( other.mAccelerationStructureBindings ) )
    , mTextureBindings(std::move(other.mTextureBindings))
	, mImageBindings(std::move(other.mImageBindings))
	, mBufferBindings(std::move(other.mBufferBindings))
    , mSpecializationConstants( std::move( other.mSpecializationConstants ) )
{
	other.mDescriptorPool = VK_NULL_HANDLE;
}

MaterialPass<Vulkan>::~MaterialPass()
{
	if (mDescriptorPool != VK_NULL_HANDLE)
	{
		//if (mDescriptorSet != VK_NULL_HANDLE)
		//	vkFreeDescriptorSets(mVulkan.m_VulkanDevice, mDescriptorPool, 1, &mDescriptorSet); only if VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
		vkDestroyDescriptorPool(mVulkan.m_VulkanDevice, mDescriptorPool, nullptr);
	}

	mDynamicPipelineLayout.Destroy(mVulkan);

	for(auto& layout: mDynamicDescriptorSetLayouts)
		vkDestroyDescriptorSetLayout(mVulkan.m_VulkanDevice, layout, nullptr);
}

bool MaterialPass<Vulkan>::UpdateDescriptorSets(uint32_t bufferIdx)
{
    // TODO: All these need to become vectors that can dynamically grow
	static const int cMAX_WRITES = 32;
	static const int cMAX_IMAGE_INFOS = 1048;
	static const int cMAX_BUFFER_INFOS = 1048;
	static const int cMAX_ACCELERATIONSTRUCTURE_INFOS = 32;
	std::array<VkWriteDescriptorSet, cMAX_WRITES> writeInfo{/*zero it*/ };
	std::array<VkDescriptorImageInfo, cMAX_IMAGE_INFOS> imageInfo{/*zero it*/ };
    std::array<VkDescriptorBufferInfo, cMAX_BUFFER_INFOS> bufferInfoFixed/*not initialized*/;
    std::vector<VkDescriptorBufferInfo> bufferInfoDynamic;

	//	std::vector<m_descriptorSetBindings>

	uint32_t writeInfoIdx = 0;
	uint32_t imageInfoCount = 0;
	uint32_t bufferInfoCount = 0;
    const size_t numDescriptorSetsPerFrame = GetShaderPass().GetDescriptorSetLayouts().size();
    const auto descriptorSetBaseIdx = bufferIdx * numDescriptorSetsPerFrame;

	// Go through the textures first
	for (const auto& textureBinding : mTextureBindings)
	{
        uint32_t setIndex = textureBinding.second.setIndex;
        uint32_t bindingIndex = textureBinding.second.setBinding.index;
        VkDescriptorType bindingType = EnumToVk(textureBinding.second.setBinding.type);

		uint32_t numTexToBind = textureBinding.second.setBinding.isArray ? (uint32_t)textureBinding.first.size() : 1;
		uint32_t texIndex = textureBinding.second.setBinding.isArray ? 0 : (bufferIdx % textureBinding.first.size());

		writeInfo[writeInfoIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo[writeInfoIdx].descriptorType = bindingType;
		writeInfo[writeInfoIdx].dstSet = mDescriptorSets[descriptorSetBaseIdx + setIndex];
		writeInfo[writeInfoIdx].dstBinding = bindingIndex;
		writeInfo[writeInfoIdx].dstArrayElement = 0;
		writeInfo[writeInfoIdx].descriptorCount = numTexToBind;
		writeInfo[writeInfoIdx].pBufferInfo = nullptr;
		writeInfo[writeInfoIdx].pImageInfo = &imageInfo[imageInfoCount];
		for (uint32_t t = 0; t < numTexToBind; ++t, ++imageInfoCount, ++texIndex)
        {
            if (imageInfoCount >= cMAX_IMAGE_INFOS)
            {
                LOGE("Max number (%d) of VkDescriptorImageInfo elements reached!", cMAX_IMAGE_INFOS);
                assert(0);
                return false;
            }

            imageInfo[imageInfoCount] = apiCast<Vulkan>(textureBinding.first[texIndex])->GetVkDescriptorImageInfo();
            assert(imageInfo[imageInfoCount].imageView != VK_NULL_HANDLE);
        }

		++writeInfoIdx;
        if (writeInfoIdx >= cMAX_WRITES)
        {
            LOGE("Max number (%d) of VkWriteDescriptorSet elements reached!", cMAX_WRITES);
            assert(0);
            return false;
        }
	}

	// Go through the images
	for (const auto& imageBinding : mImageBindings)
	{
        uint32_t setIndex = imageBinding.second.setIndex;
        uint32_t bindingIndex = imageBinding.second.setBinding.index;
		VkDescriptorType bindingType =  EnumToVk(imageBinding.second.setBinding.type);
		assert(bindingType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && bindingType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);	// combined image sampler or sampled image should go through mTextureBindings
		//assert(imageBinding.first.imageLayout == VK_IMAGE_LAYOUT_GENERAL);

		uint32_t numImgToBind = imageBinding.second.setBinding.isArray ? (uint32_t)imageBinding.first.size() : 1;
		uint32_t imgIndex = imageBinding.second.setBinding.isArray ? 0 : (bufferIdx % imageBinding.first.size());

		writeInfo[writeInfoIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo[writeInfoIdx].descriptorType = bindingType;
		writeInfo[writeInfoIdx].dstSet = mDescriptorSets[descriptorSetBaseIdx + setIndex];
		writeInfo[writeInfoIdx].dstBinding = bindingIndex;
		writeInfo[writeInfoIdx].dstArrayElement = 0;
		writeInfo[writeInfoIdx].descriptorCount = numImgToBind;
		writeInfo[writeInfoIdx].pBufferInfo = nullptr;
		writeInfo[writeInfoIdx].pImageInfo = &imageInfo[imageInfoCount];
		for (uint32_t t = 0; t < numImgToBind; ++t, ++imageInfoCount, ++imgIndex)
		{
            if (imageInfoCount >= cMAX_IMAGE_INFOS)
            {
                LOGE("Max number (%d) of VkDescriptorImageInfo elements reached!", cMAX_IMAGE_INFOS);
                assert(0);
                return false;
            }

            imageInfo[imageInfoCount].sampler = VK_NULL_HANDLE;
			imageInfo[imageInfoCount].imageView = imageBinding.first[imgIndex].imageView;
			imageInfo[imageInfoCount].imageLayout = imageBinding.first[imgIndex].imageLayout;
            assert(imageBinding.first[imgIndex].imageView != VK_NULL_HANDLE);
		}
		++writeInfoIdx;

        if (writeInfoIdx >= cMAX_WRITES)
        {
            LOGE("Max number (%d) of VkWriteDescriptorSet elements reached!", cMAX_WRITES);
            assert(0);
            return false;
        }
	}

    // Now do the buffers

    for (const auto& bufferBinding : mBufferBindings)
    {
        bufferInfoCount += bufferBinding.second.setBinding.isArray ? (uint32_t) bufferBinding.first.size() : 1;
    }
    auto* pBufferInfo = bufferInfoFixed.data();
    if (bufferInfoCount > cMAX_BUFFER_INFOS)
    {
        bufferInfoDynamic.resize( bufferInfoCount );
        pBufferInfo = bufferInfoDynamic.data();
    }

    for (const auto& bufferBinding : mBufferBindings)
    {
        uint32_t setIndex = bufferBinding.second.setIndex;
        uint32_t bindingIndex = bufferBinding.second.setBinding.index;
		VkDescriptorType bindingType =  EnumToVk(bufferBinding.second.setBinding.type);

		uint32_t numBuffersToBind = bufferBinding.second.setBinding.isArray ? (uint32_t)bufferBinding.first.size() : 1;
		uint32_t bufferIndex = bufferBinding.second.setBinding.isArray ? 0 : (bufferIdx % bufferBinding.first.size());

		writeInfo[writeInfoIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo[writeInfoIdx].descriptorType = bindingType;
		writeInfo[writeInfoIdx].dstSet = mDescriptorSets[descriptorSetBaseIdx + setIndex];
		writeInfo[writeInfoIdx].dstBinding = bindingIndex;
		writeInfo[writeInfoIdx].dstArrayElement = 0;
		writeInfo[writeInfoIdx].descriptorCount = numBuffersToBind;
		writeInfo[writeInfoIdx].pBufferInfo = pBufferInfo;
		writeInfo[writeInfoIdx].pImageInfo = nullptr;

		for (uint32_t t = 0; t < numBuffersToBind; ++t, ++bufferIndex)
		{
			pBufferInfo->buffer = bufferBinding.first[bufferIndex].buffer();
            pBufferInfo->offset = bufferBinding.first[bufferIndex].offset();
            pBufferInfo->range = VK_WHOLE_SIZE;
            ++pBufferInfo;
		}

		++writeInfoIdx;
        if (writeInfoIdx >= cMAX_WRITES)
        {
            LOGE("Max number (%d) of VkWriteDescriptorSet elements reached!", cMAX_WRITES);
            assert(0);
            return false;
        }
	}

#if VK_KHR_acceleration_structure
	// And the acceleration structures
	std::array<VkWriteDescriptorSetAccelerationStructureKHR, cMAX_ACCELERATIONSTRUCTURE_INFOS> accelerationStructureInfo{};
	uint32_t accelerationStructureCount = 0;

	for (const auto& accelerationBinding : mAccelerationStructureBindings)
	{
        uint32_t setIndex = accelerationBinding.second.setIndex;
        uint32_t bindingIndex = accelerationBinding.second.setBinding.index;
		VkDescriptorType bindingType =  EnumToVk(accelerationBinding.second.setBinding.type);

		uint32_t numAccelToBind = accelerationBinding.second.setBinding.isArray ? (uint32_t)accelerationBinding.first.size() : 1;
		uint32_t accelIndex = accelerationBinding.second.setBinding.isArray ? 0 : (bufferIdx < accelerationBinding.first.size() ? bufferIdx : 0);

        if (writeInfoIdx >= cMAX_WRITES)
        {
            LOGE("Max number (%d) of VkWriteDescriptorSet elements reached!", cMAX_WRITES);
            assert(0);
            return false;
        }

        if (accelerationStructureCount >= cMAX_ACCELERATIONSTRUCTURE_INFOS)
        {
            LOGE("Max number (%d) of VkWriteDescriptorSetAccelerationStructureKHR elements reached!", cMAX_ACCELERATIONSTRUCTURE_INFOS);
            assert(0);
            return false;
        }
        
        const auto* pAs = apiCast<Vulkan>(accelerationBinding.first[accelIndex]);
		accelerationStructureInfo[accelerationStructureCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		accelerationStructureInfo[accelerationStructureCount].accelerationStructureCount = 1;
		accelerationStructureInfo[accelerationStructureCount].pAccelerationStructures = &pAs->GetVkAccelerationStructure();

		writeInfo[writeInfoIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo[writeInfoIdx].descriptorType = bindingType;//VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
		writeInfo[writeInfoIdx].dstSet = mDescriptorSets[descriptorSetBaseIdx + setIndex];
		writeInfo[writeInfoIdx].dstBinding = bindingIndex;
		writeInfo[writeInfoIdx].dstArrayElement = 0;
		writeInfo[writeInfoIdx].descriptorCount = accelerationStructureInfo[accelerationStructureCount].accelerationStructureCount;
		writeInfo[writeInfoIdx].pBufferInfo = nullptr;
		writeInfo[writeInfoIdx].pImageInfo = nullptr;
		writeInfo[writeInfoIdx].pNext = &accelerationStructureInfo[accelerationStructureCount];
		++writeInfoIdx;
		++accelerationStructureCount;

	}
#endif // VK_KHR_acceleration_structure

	// LOGI("Updating Descriptor Set (bufferIdx %d) with %d objects", bufferIdx, writeInfoIdx);
	vkUpdateDescriptorSets(mVulkan.m_VulkanDevice, writeInfoIdx, writeInfo.data(), 0, NULL);
	// LOGI("Descriptor Set Updated!");

	return true;
}

bool MaterialPass<Vulkan>::UpdateDescriptorSetBinding(uint32_t bufferIdx, const std::string& bindingName, const Texture<Vulkan>& newTexture) const
{
    std::array<VkWriteDescriptorSet, 1>  writeInfo{ {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET} };
	std::array<VkDescriptorImageInfo, 1> imageInfo{/*zero it*/ };

	uint32_t writeInfoIdx = 0;
	uint32_t imageInfoCount = 0;

    for (int setIdx = 0; const auto & setLayout : GetShaderPass().GetDescriptorSetLayouts())
    {
        const auto& nameToBinding = setLayout.GetNameToBinding();
        const auto bindingIt = nameToBinding.find( bindingName );
        if (bindingIt != nameToBinding.end())
        {
            const auto& descriptorSet = GetVkDescriptorSet( bufferIdx, setIdx );
            VkDescriptorType bindingType = EnumToVk(bindingIt->second.type);
            assert( bindingType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || bindingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || bindingType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

            uint32_t bindingIndex = bindingIt->second.index;

            writeInfo[writeInfoIdx].descriptorType = bindingType;
            writeInfo[writeInfoIdx].dstSet = descriptorSet;
            writeInfo[writeInfoIdx].dstBinding = bindingIndex;
            writeInfo[writeInfoIdx].dstArrayElement = 0;
            writeInfo[writeInfoIdx].descriptorCount = 1;
            writeInfo[writeInfoIdx].pBufferInfo = nullptr;
            writeInfo[writeInfoIdx].pImageInfo = &imageInfo[imageInfoCount];
            imageInfo[imageInfoCount] = newTexture.GetVkDescriptorImageInfo();

            ++imageInfoCount;
            ++writeInfoIdx;

            vkUpdateDescriptorSets( mVulkan.m_VulkanDevice, writeInfoIdx, writeInfo.data(), 0, NULL );
            return true;
        }
        ++setIdx;
    }
    assert( 0 && "Binding name not found" );
    return false;
}


ImageInfo<Vulkan>::ImageInfo(const Texture<Vulkan>& t)
: image(t.GetVkImage())
, imageView(t.GetVkImageView())
, imageViewNumMips(t.MipLevels)
, imageViewFirstMip(t.FirstMip)
, imageLayout( t.GetVkImageLayout() )
{
}

ImageInfo<Vulkan>::ImageInfo(const TextureBase& t) : ImageInfo( apiCast<Vulkan>(t) )
{
}

ImageInfo<Vulkan>::ImageInfo(const ImageInfo& src)
: image(src.image)
, imageView(src.imageView)
, imageLayout( src.imageLayout )
, imageViewNumMips( src.imageViewNumMips )
, imageViewFirstMip( src.imageViewFirstMip )
{
}

ImageInfo<Vulkan>::ImageInfo( ImageInfo&& src ) noexcept
    : ImageInfoBase()
    , image( src.image )
    , imageView( src.imageView )
    , imageLayout( src.imageLayout )
    , imageViewNumMips( src.imageViewNumMips )
    , imageViewFirstMip( src.imageViewFirstMip )
{
    src.image = VK_NULL_HANDLE;
    src.imageView = VK_NULL_HANDLE;
    src.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    src.imageViewNumMips = 0;
    src.imageViewFirstMip = 0;
}

ImageInfo<Vulkan>& ImageInfo<Vulkan>::operator=(ImageInfo&& src) noexcept
{
    if (this != &src)
    {
        image = src.image;
        imageView = src.imageView;
        imageLayout = src.imageLayout;
        imageViewNumMips = src.imageViewNumMips;
        imageViewFirstMip = src.imageViewFirstMip;
        src.image = VK_NULL_HANDLE;
        src.imageView = VK_NULL_HANDLE;
        src.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        src.imageViewNumMips = 0;
        src.imageViewFirstMip = 0;
    }
    return *this;
}

ImageInfo<Vulkan>& ImageInfo<Vulkan>::operator=( const ImageInfo& src) noexcept
{
    if (this != &src)
    {
        image = src.image;
        imageView = src.imageView;
        imageLayout = src.imageLayout;
        imageViewNumMips = src.imageViewNumMips;
        imageViewFirstMip = src.imageViewFirstMip;
    }
    return *this;
}
