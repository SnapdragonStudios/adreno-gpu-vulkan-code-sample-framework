//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <optional>
#include "vulkan/vulkan.hpp"
#include "../descriptorSetLayout.hpp"
#include "shader.hpp"
#include "pipelineLayout.hpp"
#include "specializationConstants.hpp"
#include "specializationConstantsLayout.hpp"
#include "../materialManager.hpp"
#include "../materialPass.hpp"


// Forward declarations
class TextureBase;
template<typename T_GFXAPI> struct ImageInfo;
template<typename T_GFXAPI> class PipelineLayout;
template<typename T_GFXAPI> class ShaderPass;
template<typename T_GFXAPI> class Texture;

using VkBufferAndOffset = BufferAndOffset<Vulkan>;
using PerFrameBufferVulkan = PerFrameBuffer<Vulkan>;


/// Reference to a VkImage.
/// Does not have ownership over referenced VkImage or VkImageView and lifetime of those object should be longer than the referencing @ImageInfo (no reference counting).
/// @ingroup Material
template<>
struct ImageInfo<Vulkan> : public ImageInfoBase {
    ImageInfo() noexcept : ImageInfoBase() {}
    ImageInfo( ImageInfo<Vulkan>&&) noexcept;
    ImageInfo& operator=(ImageInfo<Vulkan>&&) noexcept;
    ImageInfo& operator=(const ImageInfo<Vulkan>&) noexcept;
    ImageInfo(const ImageInfo<Vulkan>&);
    ImageInfo(const Texture<Vulkan>&);
    ImageInfo(const TextureBase&);
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    uint32_t imageViewNumMips = 0;
    uint32_t imageViewFirstMip = 0;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL;
};



/// An instance of a ShaderPass material.
/// Template class specialization for Vulkan
/// @ingroup Material
template<>
class MaterialPass<Vulkan> final : public MaterialPassBase
{
    MaterialPass(const MaterialPass<Vulkan>&) = delete;
    MaterialPass<Vulkan>& operator=(const MaterialPass<Vulkan>&) = delete;
public:

    typedef std::vector <ImageInfo<Vulkan>> tPerFrameImageInfo;

    typedef std::vector <std::pair<MaterialManagerBase::tPerFrameTexInfo,   DescriptorSetLayoutBase::DescriptorBinding>> tTextureBindings;
    typedef std::vector <std::pair<tPerFrameImageInfo,                  DescriptorSetLayoutBase::DescriptorBinding>> tImageBindings;
    typedef std::vector <std::pair<PerFrameBufferVulkan,                   DescriptorSetLayoutBase::DescriptorBinding>> tBufferBindings;
    typedef std::vector <std::pair<MaterialManagerBase::tPerFrameAccelerationStructure, DescriptorSetLayoutBase::DescriptorBinding>> tAccelerationStructureBindings;

    MaterialPass(Vulkan& vulkan, const ShaderPass<Vulkan>&, VkDescriptorPool, std::vector<VkDescriptorSet>, std::vector<VkDescriptorSetLayout> dynamicDescriptorSetLayouts, tTextureBindings, tImageBindings, tBufferBindings, tAccelerationStructureBindings, SpecializationConstants<Vulkan>) noexcept;
    MaterialPass(MaterialPass<Vulkan>&&) noexcept;
    ~MaterialPass();

    const auto& GetShaderPass() const               { return apiCast<Vulkan>( mShaderPass ); }

    /// Get the descriptor set for the (numbered) frame buffer index, allows for a single descriptor set identical for all frames if required.
    const auto& GetVkDescriptorSet(uint32_t bufferIndex, int32_t setIndex) const { return mDescriptorSets[mDescriptorSets.size() > 1 ? bufferIndex : 0]; }
    const std::span<const VkDescriptorSet> GetVkDescriptorSets( uint32_t bufferIndex ) const { return {&mDescriptorSets[(bufferIndex % mNumBuffers) * mNumDescriptorSetsPerBuffer], mNumDescriptorSetsPerBuffer}; }
    const auto& GetVkDescriptorSets() const         { return mDescriptorSets; }
    const auto& GetPipelineLayout() const           { return mDynamicPipelineLayout; }
    const auto& GetSpecializationConstants() const  { return mSpecializationConstants; };

    const auto& GetTextureBindings() const          { return mTextureBindings; }
    const auto& GetImageBindings() const            { return mImageBindings; }
    const auto& GetBufferBindings() const           { return mBufferBindings; }

    bool UpdateDescriptorSets(uint32_t bufferIdx);
    bool UpdateDescriptorSetBinding(uint32_t bufferIdx, const std::string& bindingName, const Texture<Vulkan>& newTexture) const;

protected:
    Vulkan& mVulkan;

    // Helpers for size of mDescriptorSets
    const uint32_t mNumDescriptorSetsPerBuffer;                 ///< number of descriptor sets needed by the shader(pass).  Usually 1 but some shaders will use more then one secriptor set.
    const uint32_t mNumBuffers;                                 ///< Number of buffers worth of descriptors (may be 1, or number of framebuffers, or something else)

    // Vulkan objects
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;               ///< array of descriptor sets (mNumDescriptorSetsPerBuffer * mNumBuffers))
    std::vector<VkDescriptorSetLayout> mDynamicDescriptorSetLayouts;///< array of descriptor set layouts specific for to this materialPass (usually they are shared across all materials with a specific shader, except in the case of descriptor sets that are 'dynamically' sized to fit the material specific contents)
    PipelineLayout<Vulkan> mDynamicPipelineLayout;              ///< pipeline layout specific to this materiaPass (usually shaderPass contains the pipeline layout but for materials with 'dynamic' descriptor set layouts we have to have a unique pipeline per materialPass
    SpecializationConstants<Vulkan> mSpecializationConstants;   ///< block of specialization constants for this material pass

    tTextureBindings mTextureBindings;                          ///< Images (textures) (with sampler) considered readonly
    tImageBindings mImageBindings;                              ///< Images that may be bound as writable (or read/write).
    tBufferBindings mBufferBindings;
    tAccelerationStructureBindings mAccelerationStructureBindings;
};
