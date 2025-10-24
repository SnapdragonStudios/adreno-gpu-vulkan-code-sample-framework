//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <optional>
#include "shader.hpp"
#include "../materialManager.hpp"
#include "../materialPass.hpp"

using MaterialPassDx12 = MaterialPass<Dx12>;

// Forward declarations
class TextureBase;
struct DescriptorTableHandle;
template<typename T_GFXAPI> struct ImageInfo;
template<typename T_GFXAPI> class PipelineLayout;
template<typename T_GFXAPI> class ShaderPass;
template<typename T_GFXAPI> class SpecializationConstants;
template<typename T_GFXAPI> class Texture;


struct DescriptorBinding
{
    enum class DescriptorType {
        Constant,
        CBV,
        SRV,
        UAV,
        Descriptor
    } descriptorType;
    UINT bindingIndex;
    UINT count;
};

struct RootItem {
    DescriptorBinding binding;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
};


/// Reference to a texture image.
/// Does not have ownership over referenced image or view and lifetime of those object should be longer than the referencing @ImageInfo (no reference counting).
/// @ingroup Material
template<>
struct ImageInfo<Dx12> : public ImageInfoBase {
    ImageInfo() noexcept : ImageInfoBase() {}
    ImageInfo(ImageInfo&&) noexcept;
    ImageInfo& operator=(ImageInfo&&) noexcept;
    ImageInfo& operator=(const ImageInfo&) = delete;
    ImageInfo(const ImageInfo&);
    ImageInfo(const Texture<Dx12>&);
    ImageInfo(const TextureBase&);
    uint32_t imageViewNumMips = 0;
    uint32_t imageViewFirstMip = 0;
};


/// An instance of a ShaderPass material.
/// Template class specialization for Vulkan
/// @ingroup Material
template<>
class MaterialPass<Dx12> : public MaterialPassBase
{
    MaterialPass(const MaterialPass<Dx12>&) = delete;
    MaterialPass<Dx12>& operator=(const MaterialPass<Dx12>&) = delete;
    using ImageInfo = ImageInfo<Dx12>;
public:

    typedef std::vector <ImageInfo> tPerFrameImageInfo;

    typedef std::vector <std::pair<MaterialManagerBase::tPerFrameTexInfo, DescriptorTypeAndLocation>> tTextureBindings;
    typedef std::vector <std::pair<tPerFrameImageInfo, DescriptorTypeAndLocation>> tImageBindings;
    typedef std::vector <std::pair<PerFrameBuffer<Dx12>, DescriptorTypeAndLocation>> tBufferBindings;
    typedef std::vector <std::pair<MaterialManagerBase::tPerFrameAccelerationStructure, DescriptorTypeAndLocation>> tAccelerationStructureBindings;

    MaterialPass() noexcept = delete;
    MaterialPass(Dx12& gfxapi, const ShaderPass<Dx12>&/*, VkDescriptorPool, std::vector<DescriptorTableHandle>&&/*, std::vector<VkDescriptorSetLayout>,*/, tTextureBindings, tImageBindings, tBufferBindings, tAccelerationStructureBindings, SpecializationConstants<Dx12>) noexcept;
    MaterialPass(MaterialPass<Dx12>&&) noexcept;
    ~MaterialPass();

    const auto& GetShaderPass() const { return apiCast<Dx12>( mShaderPass ); }

    const auto& GetRootData() const         { return mRootData; }
    const auto& GetDescriptorTables() const { return mDescriptorTables; }
    const SpecializationConstants<Dx12>& GetSpecializationConstants() const;

    bool UpdateDescriptorSets(uint32_t bufferIdx);

protected:
    Dx12& mGfxApi;

    // Dx12 objects
    std::vector<RootItem> mRootData;                    ///< Root descriptor table data
    std::vector<DescriptorTableHandle> mDescriptorTables;///< Non root descriptor tables

    tTextureBindings mTextureBindings;                  ///< Images (textures) (with sampler) considered readonly
    tImageBindings mImageBindings;                      ///< Images that may be bound as writable (or read/write).
    tBufferBindings mBufferBindings;
    tAccelerationStructureBindings mAccelerationStructureBindings;

};


