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
#include "vulkan/material.hpp"
#include "texture/vulkan/texture.hpp"


MaterialPass::MaterialPass(Vulkan& vulkan, const ShaderPass<Vulkan>& shaderPass, VkDescriptorPool&& descriptorPool, std::vector<VkDescriptorSet>&& descriptorSets, std::vector<VkDescriptorSetLayout>&& dynamicDescriptorSetLayouts, tTextureBindings&& textureBindings, tImageBindings&& imageBindings, tBufferBindings&& bufferBindings, tAccelerationStructureBindings&& accelerationStructureBindings, SpecializationConstants&& specializationConstants )
	: mShaderPass(shaderPass)
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

MaterialPass::MaterialPass(MaterialPass&& other) noexcept
	: mShaderPass(other.mShaderPass)
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

MaterialPass::~MaterialPass()
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

ImageInfo::ImageInfo(const TextureT<Vulkan>& t)
	: image(t.GetVkImage())
	, imageView(t.GetVkImageView())
	, imageViewNumMips(t.MipLevels)
	, imageViewFirstMip(t.FirstMip)
    , imageLayout( t.GetVkImageLayout() )
{
}

ImageInfo::ImageInfo(const Texture& t) : ImageInfo( apiCast<Vulkan>(t) )
{
}

ImageInfo::ImageInfo(const ImageInfo& src)
	: image(src.image)
	, imageView(src.imageView)
	, imageLayout( src.imageLayout )
	, imageViewNumMips( src.imageViewNumMips )
	, imageViewFirstMip( src.imageViewFirstMip )
{
}

ImageInfo::ImageInfo( ImageInfo&& src ) noexcept
	: image( src.image )
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

ImageInfo& ImageInfo::operator=(ImageInfo&& src) noexcept
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

bool MaterialPass::UpdateDescriptorSets(uint32_t bufferIdx)
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
    const size_t numDescriptorSetsPerFrame = mShaderPass.GetDescriptorSetLayouts().size();
    const auto descriptorSetBaseIdx = bufferIdx * numDescriptorSetsPerFrame;

	// Go through the textures first
	for (const auto& textureBinding : mTextureBindings)
	{
        uint32_t setIndex = textureBinding.second.setIndex;
        uint32_t bindingIndex = textureBinding.second.setBinding.index;
        VkDescriptorType bindingType = textureBinding.second.setBinding.type;

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
		VkDescriptorType bindingType = imageBinding.second.setBinding.type;
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
		VkDescriptorType bindingType = bufferBinding.second.setBinding.type;

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
			pBufferInfo->buffer = bufferBinding.first[bufferIndex].buffer;
            pBufferInfo->offset = bufferBinding.first[bufferIndex].offset;
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
		VkDescriptorType bindingType = accelerationBinding.second.setBinding.type;

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
        
        const VkAccelerationStructureKHR * pAs = &accelerationBinding.first[accelIndex];
		accelerationStructureInfo[accelerationStructureCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		accelerationStructureInfo[accelerationStructureCount].accelerationStructureCount = 1;
		accelerationStructureInfo[accelerationStructureCount].pAccelerationStructures = pAs;

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

bool MaterialPass::UpdateDescriptorSetBinding(uint32_t bufferIdx, const std::string& bindingName, const TextureT<Vulkan>& newTexture) const
{
    std::array<VkWriteDescriptorSet, 1>  writeInfo{ {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET} };
	std::array<VkDescriptorImageInfo, 1> imageInfo{/*zero it*/ };

	uint32_t writeInfoIdx = 0;
	uint32_t imageInfoCount = 0;

    for (int setIdx = 0; const auto & setLayout : mShaderPass.GetDescriptorSetLayouts())
    {
        const auto& nameToBinding = setLayout.GetNameToBinding();
        const auto bindingIt = nameToBinding.find( bindingName );
        if (bindingIt != nameToBinding.end())
        {
            const auto& descriptorSet = GetVkDescriptorSet( bufferIdx, setIdx );
            VkDescriptorType bindingType = bindingIt->second.type;
            assert( bindingType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || bindingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );

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


//
// Material class implementation
//

Material::Material(const ShaderT<Vulkan>& shader, uint32_t numFramebuffers)
	: m_shader(shader)
	, m_numFramebuffers(numFramebuffers)
{
	assert(m_numFramebuffers > 0);
}

Material::Material(Material&& other) noexcept
	: m_shader(other.m_shader)
	, m_materialPassNamesToIndex(std::move(other.m_materialPassNamesToIndex))
	, m_materialPasses(std::move(other.m_materialPasses))
	, m_numFramebuffers(other.m_numFramebuffers)
{
}

Material::~Material()
{}


const MaterialPass* Material::GetMaterialPass(const std::string& passName) const
{
	auto it = m_materialPassNamesToIndex.find(passName);
	if (it != m_materialPassNamesToIndex.end())
	{
		return &m_materialPasses[it->second];
	}
	return nullptr;
}

bool Material::UpdateDescriptorSets(uint32_t bufferIdx)
{
	// Just go through and update the MaterialPass descriptor sets.
	// Could be much smarter (and could handle failures better than just continuing and reporting something went wrong)
	bool success = true;
	for (auto& materialPass : m_materialPasses)
	{
		success &= materialPass.UpdateDescriptorSets(bufferIdx);
	}
	return success;
}

bool Material::UpdateDescriptorSetBinding(uint32_t bufferIdx, const std::string& bindingName, const TextureT<Vulkan>& newTexture) const
{
	bool success = true;
	for (auto& materialPass : m_materialPasses)
	{
		success &= materialPass.UpdateDescriptorSetBinding(bufferIdx, bindingName, newTexture);
	}
	return success;
}

bool SpecializationConstants::Init( const SpecializationConstantsLayout<Vulkan>& layout, const std::span<VertexElementData> constants )
{
    const auto& layoutMapEntry = layout.GetVkSpecializationMapEntry();
    assert( layoutMapEntry.size() == constants.size() );
    if (layoutMapEntry.empty())
    {
        mSpecializationData.reset();
        return true;
    }

    std::span<std::byte> specializationConstantsRaw { new std::byte[layout.GetBufferSize()], layout.GetBufferSize() };    // unsafe - raw pointer in the span!

    for (auto constantIdx = 0; constantIdx < layoutMapEntry.size(); ++constantIdx)
    {
        const auto& constantLayout = layoutMapEntry[constantIdx];

        // copy the loaded constant data into the constant buffer
        std::span constantDataRaw = constants[constantIdx].getUnsafeData();
        assert( constantLayout.size == constantDataRaw.size() );
        std::copy( constantDataRaw.begin(), constantDataRaw.end(), specializationConstantsRaw.begin() + constantLayout.offset );
    }

    VkSpecializationInfo vkSpecializationInfo {};
    vkSpecializationInfo.mapEntryCount = (uint32_t) layout.GetVkSpecializationMapEntry().size();
    vkSpecializationInfo.pMapEntries = layout.GetVkSpecializationMapEntry().data();
    vkSpecializationInfo.dataSize = specializationConstantsRaw.size();
    vkSpecializationInfo.pData = specializationConstantsRaw.data(); // move ownership of the allocated buffer.

    mSpecializationData.emplace().specializationInfo = vkSpecializationInfo;

    return true;
}

SpecializationConstants::VulkanSpecializationData::VulkanSpecializationData( SpecializationConstants::VulkanSpecializationData&& other ) noexcept : specializationInfo( other.specializationInfo )/*dumb move*/
{
    other.specializationInfo.mapEntryCount = 0;
    other.specializationInfo.pMapEntries = nullptr;
    other.specializationInfo.dataSize = 0;
    other.specializationInfo.pData = nullptr;
}
