//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "material.hpp"
#include "shader.hpp"
#include "../specializationConstants.hpp"
#include <array>
#include "system/os_common.h"
#include "texture/dx12/texture.hpp"


MaterialPass<Dx12>::MaterialPass(Dx12& gfxapi, const ShaderPass<Dx12>& shaderPass/*, VkDescriptorPool descriptorPool, std::vector<DescriptorTableHandle>&& descriptorTables, std::vector<VkDescriptorSetLayout> dynamicDescriptorSetLayouts*/, tTextureBindings textureBindings, tImageBindings imageBindings, tBufferBindings bufferBindings, tAccelerationStructureBindings accelerationStructureBindings, SpecializationConstants<Dx12> specializationConstants ) noexcept
	: MaterialPassBase(shaderPass)
    , mGfxApi( gfxapi )
    , mTextureBindings(std::move(textureBindings))
	, mImageBindings(std::move(imageBindings))
	, mBufferBindings(std::move(bufferBindings))
    , mAccelerationStructureBindings( std::move( accelerationStructureBindings ) )
{
}

MaterialPass<Dx12>::MaterialPass(MaterialPass<Dx12>&& other) noexcept
    : MaterialPassBase(std::move(other))
	, mGfxApi( other.mGfxApi )
    , mRootData(std::move(other.mRootData))
    , mDescriptorTables(std::move(other.mDescriptorTables))
    , mTextureBindings(std::move(other.mTextureBindings))
    , mImageBindings(std::move(other.mImageBindings))
    , mBufferBindings(std::move(other.mBufferBindings))
    , mAccelerationStructureBindings( std::move( other.mAccelerationStructureBindings ) )
{
}

MaterialPass<Dx12>::~MaterialPass()
{
    for (auto& descriptorTable: mDescriptorTables)
    {
        mGfxApi.FreeShaderResourceViewDescriptors(std::move(descriptorTable));
    }
}

const SpecializationConstants<Dx12>& MaterialPass<Dx12>::GetSpecializationConstants() const
{
    static SpecializationConstants<Dx12> empty; //not available on Dx12
    return empty;
}

ImageInfo<Dx12>::ImageInfo(const Texture<Dx12>& t)
	: ImageInfoBase()
    , imageViewNumMips(t.MipLevels)
	, imageViewFirstMip(t.FirstMip)
{
}

ImageInfo<Dx12>::ImageInfo(const TextureBase& t) : ImageInfo( apiCast<Dx12>(t) )
{
}

ImageInfo<Dx12>::ImageInfo(const ImageInfo& src)
    : ImageInfoBase()
    , imageViewNumMips(src.imageViewNumMips)
	, imageViewFirstMip( src.imageViewFirstMip )
{
}

ImageInfo<Dx12>::ImageInfo( ImageInfo&& src ) noexcept
	: ImageInfoBase()
    , imageViewNumMips( src.imageViewNumMips )
	, imageViewFirstMip( src.imageViewFirstMip )
{
	src.imageViewNumMips = 0;
	src.imageViewFirstMip = 0;
}

ImageInfo<Dx12>& ImageInfo<Dx12>::operator=(ImageInfo<Dx12>&& src) noexcept
{
	if (this != &src)
	{
		src.imageViewNumMips = 0;
		src.imageViewFirstMip = 0;
	}
	return *this;
}

bool MaterialPass<Dx12>::UpdateDescriptorSets(uint32_t bufferIdx)
{
    const auto& descriptorTableLayouts = GetShaderPass().GetDescriptorSetLayouts();
    mDescriptorTables.reserve(descriptorTableLayouts.size());

    for (bool root=true; auto& descriptorTableLayout : descriptorTableLayouts)
    {
        if (root)
        {
            // The 'root' descriptor table allocation isnt used (root descriptor table is handled differently), but we make a dummy allocation anyhow! ///TODO: remove this allocation
            const auto& layout = descriptorTableLayout.GetLayout<true>();
            mDescriptorTables.emplace_back(mGfxApi.AllocateShaderResourceViewDescriptors(layout.root.size()));
            // Pre-size the rootdata array (so we can write 'out of order')
            mRootData.resize(layout.root.size());
        }
        else
        {
            const auto& layout = descriptorTableLayout.GetLayout<false>();
            mDescriptorTables.emplace_back(mGfxApi.AllocateShaderResourceViewDescriptors(layout.ranges.size()));
        }
        root = false;
    }

    // Go through the textures first
    for (const auto& textureBinding : mTextureBindings)
    {
        uint32_t tableIndex = textureBinding.second.setIndex;
        uint32_t bindingIndex = textureBinding.second.index;
        uint32_t numTexToBind = textureBinding.second.isArray ? (uint32_t)textureBinding.first.size() : 1;
        uint32_t texIndex = textureBinding.second.isArray ? 0 : (bufferIdx < textureBinding.first.size() ? bufferIdx : 0);

        if (tableIndex == 0)
        {
            // Add to root
            for (uint32_t t = 0; t < numTexToBind; ++t, ++bindingIndex, ++texIndex)
            {
                const auto& texture = apiCast<Dx12>( textureBinding.first[texIndex] );
                mRootData[bindingIndex] = RootItem{.binding = {.descriptorType = DescriptorBinding::DescriptorType::SRV,
                                                   .bindingIndex = bindingIndex,
                                                   .count = 1},
                                                   .gpuAddress = texture->GetResource()->GetGPUVirtualAddress()};
            }
        }
        else
        {
            for (uint32_t t = 0; t < numTexToBind; ++t, ++bindingIndex, ++texIndex)
            {
                // Create texture view
                const auto& texture = apiCast<Dx12>( textureBinding.first[texIndex] );
                mGfxApi.GetDevice()->CreateShaderResourceView( texture->GetResource(), &texture->GetResourceViewDesc(), mDescriptorTables[tableIndex].GetCpuHandle( bindingIndex ) );
            }
        }
    }

    // Now do the buffers
    for (const auto& bufferBinding : mBufferBindings)
    {
        uint32_t tableIndex = bufferBinding.second.setIndex;
        uint32_t bindingIndex = bufferBinding.second.index;
        uint32_t numBuffersToBind = bufferBinding.second.isArray ? (uint32_t)bufferBinding.first.size() : 1;
        uint32_t bufferIndex = bufferBinding.second.isArray ? 0 : (bufferIdx < bufferBinding.first.size() ? bufferIdx : 0);

        if (tableIndex == 0)
        {
            // Add to root
            for (uint32_t t = 0; t < numBuffersToBind; ++t, ++bufferIndex)
            {
                const auto& buffer = bufferBinding.first[bufferIndex];
                mRootData[bindingIndex] = RootItem{.binding = {.descriptorType = DescriptorBinding::DescriptorType::CBV,
                                                               .bindingIndex = bindingIndex,
                                                               .count = 1},
                                                               .gpuAddress = buffer.buffer()->GetGPUVirtualAddress()};
            }
        }
        else
        {
            for (uint32_t t = 0; t < numBuffersToBind; ++t, ++bufferIndex)
            {
        	    const auto& buffer = bufferBinding.first[bufferIndex];
                assert(buffer.offset() == 0 && "Buffer offset not (currently) implemented for Dx12");
                D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
                viewDesc.BufferLocation = buffer.buffer()->GetGPUVirtualAddress();
                viewDesc.SizeInBytes = buffer.buffer()->GetDesc().Width;
                mGfxApi.GetDevice()->CreateConstantBufferView(&viewDesc, mDescriptorTables[tableIndex].GetCpuHandle(bindingIndex));
            }
        }
    }

    return true;
}

