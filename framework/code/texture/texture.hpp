//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <memory>
#include "textureFormat.hpp"


// Forward declarations
class GraphicsApiBase;
template<typename T_GFXAPI> class TextureT;
template<typename T_GFXAPI> class SamplerT;


/// @brief Base class for a texture object (image with a sampler).
/// Is subclassed for each graphics API.
class Texture
{
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = TextureT<T_GFXAPI>; // make apiCast work!

    Texture() noexcept {}
    virtual ~Texture() noexcept = 0;

    virtual void Release(GraphicsApiBase* pApi) = 0;
};
inline Texture::~Texture() noexcept {}


/// @brief Texture container for a (templated) graphics api.
/// Owns memory and sampler etc associated with a single texture.
/// Template is expected to be specialized for the graphics api (Vulkan, DirectX etc)
template<typename T_GFXAPI>
class TextureT final : public Texture
{
    TextureT(const TextureT<T_GFXAPI>&) = delete;
    TextureT& operator=(const TextureT<T_GFXAPI>&) = delete;
public:
    TextureT() noexcept = delete;                                         // template class expected to be specialized
    TextureT(TextureT<T_GFXAPI>&&) noexcept = delete;                     // template class expected to be specialized
    TextureT<T_GFXAPI>& operator=(TextureT<T_GFXAPI>&&) noexcept = delete;// template class expected to be specialized

    bool IsEmpty() const { return true; }

protected:
    static_assert(sizeof(TextureT<T_GFXAPI>) != sizeof(Texture));   // Ensure this class template is specialized (and not used as-is)
};


/// @brief Base class for a sampler object.
/// Is subclassed for each graphics API.
class Sampler
{
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = SamplerT<T_GFXAPI>; // make apiCast work!
protected:
    Sampler() noexcept {}
};


/// @brief Template for sampler to be specialized by Vulkan graphics api.
template<typename T_GFXAPI>
class SamplerT final : public Sampler
{
    SamplerT(const SamplerT<T_GFXAPI>&) = delete;
    SamplerT& operator=(const SamplerT<T_GFXAPI>&) = delete;
public:
    SamplerT() noexcept = delete;                                         // template class expected to be specialized
    SamplerT(SamplerT<T_GFXAPI>&&) noexcept = delete;                     // template class expected to be specialized
    SamplerT<T_GFXAPI>& operator=(SamplerT<T_GFXAPI>&&) noexcept = delete;// template class expected to be specialized

    static_assert(sizeof(SamplerT<T_GFXAPI>) != sizeof(Sampler));   // Ensure this class template is specialized (and not used as-is)
};


enum class SamplerAddressMode {
    Undefined = 0,
    Repeat = 1,
    MirroredRepeat = 2,
    ClampEdge = 3,
    ClampBorder = 4,
    MirroredClampEdge = 5,
    End
};


enum class SamplerFilter {
    Undefined = 0,
    Nearest = 1,
    Linear = 2
};


enum class SamplerBorderColor {
    TransparentBlackFloat = 0,
    TransparentBlackInt = 1,
    OpaqueBlackFloat = 2,
    OpaqueBlackInt = 3,
    OpaqueWhiteFloat = 4,
    OpaqueWhiteInt = 5
};


/// Texture types (intended use) for CreateTextureObject
enum TEXTURE_TYPE
{
    TT_NORMAL = 0,
    TT_RENDER_TARGET,
    TT_RENDER_TARGET_WITH_STORAGE,
    TT_RENDER_TARGET_TRANSFERSRC,
    TT_RENDER_TARGET_SUBPASS,
    TT_DEPTH_TARGET,
    TT_COMPUTE_TARGET,
    TT_COMPUTE_STORAGE,
    TT_CPU_UPDATE,
    TT_SHADING_RATE_IMAGE,
    NUM_TEXTURE_TYPES
};


/// Texture flags (feature enable/disable) for CreateTextureObject
enum TEXTURE_FLAGS
{
    None = 0,
    MutableFormat = 1,
    ForceLinearTiling = 2
};


/// Parameters for CreateTextureObject
struct CreateTexObjectInfo
{
    uint32_t uiWidth = 0;
    uint32_t uiHeight = 0;
    uint32_t uiDepth = 1;	                                                // default to 2d texture.
    uint32_t uiMips = 1;                                                    // default to no mips
    uint32_t uiFaces = 1;	                                                // default to 1 face (ie not a texture array or cubemap)
    TextureFormat Format = TextureFormat::UNDEFINED;
    TEXTURE_TYPE TexType = TEXTURE_TYPE::TT_NORMAL;
    TEXTURE_FLAGS Flags = TEXTURE_FLAGS::None;
    const char* pName = nullptr;
    uint32_t Msaa = 1;                                                      ///< number of msaa samples per pixel (ie 1, 2, 4, etc).  Default to 1 (no msaa)
    SamplerFilter FilterMode = SamplerFilter::Undefined;					//default to picking from a default dependant on the texture format
    SamplerAddressMode SamplerMode = SamplerAddressMode::Undefined;	        //default to picking from a default dependant on the texture type
    bool UnNormalizedCoordinates = false;
};


/// Parameters for CreateTextureObject
struct CreateSamplerObjectInfo
{
    SamplerAddressMode  Mode = SamplerAddressMode::Repeat;
    SamplerFilter       Filter = SamplerFilter::Linear;
    SamplerBorderColor  BorderColor = SamplerBorderColor::TransparentBlackFloat;
    bool                UnnormalizedCoordinates = false;
    float               MipBias = 0.0f;
    float               MinLod = 0.0f;
    float               MaxLod = FLT_MAX;
};


/// Create texture (generally for render target usage)
template<typename T_GFXAPI>
TextureT<T_GFXAPI> CreateTextureObject(T_GFXAPI& gfxApi, const CreateTexObjectInfo& texInfo)
{
    assert(0 && "Expecting CreateTextureObject (per graphics api) to be used");
    return {};
}

/// Create texture (generally for render target usage)
template<typename T_GFXAPI>
TextureT<T_GFXAPI> CreateTextureObject(T_GFXAPI& gfxApi, uint32_t uiWidth, uint32_t uiHeight, TextureFormat Format, TEXTURE_TYPE TexType, const char* pName, uint32_t Msaa = 1, TEXTURE_FLAGS Flags = TEXTURE_FLAGS::None)
{
    CreateTexObjectInfo createInfo{};
    createInfo.uiWidth = uiWidth;
    createInfo.uiHeight = uiHeight;
    createInfo.Format = Format;
    createInfo.TexType = TexType;
    createInfo.Flags = Flags;
    createInfo.pName = pName;
    createInfo.Msaa = Msaa;
    return CreateTextureObject(gfxApi, createInfo);
}

/// Create texture (unique_ptr) (generally for render target usage).  Uses CreateTexObjectInfo structure to define texture creation parameters.
template<typename T_GFXAPI>
std::unique_ptr<Texture> CreateTextureObject(GraphicsApiBase& gfxApi, const CreateTexObjectInfo& texInfo)
{
    auto pTexture = std::make_unique<TextureT<T_GFXAPI>>();
    *pTexture = std::move(CreateTextureObject(static_cast<T_GFXAPI&>(gfxApi), texInfo));
    return pTexture;
}

/// Create texture from a memory buffer.
template<typename T_GFXAPI>
TextureT<T_GFXAPI> CreateTextureFromBuffer( T_GFXAPI& gfxApi, const void* pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, TextureFormat Format, SamplerAddressMode SamplerMode, SamplerFilter Filter, const char* pName = nullptr )
{
    assert( 0 && "Expecting CreateTextureFromBuffer (per graphics api) to be used" );
    return {};
}

/// Create texture (unique_ptr) (generally for render target usage).  Uses CreateTexObjectInfo structure to define texture creation parameters.
template<typename T_GFXAPI>
std::unique_ptr<Texture> CreateTextureFromBuffer( GraphicsApiBase& gfxApi, const void* pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, TextureFormat Format, SamplerAddressMode SamplerMode, SamplerFilter Filter, const char* pName = nullptr )
{
    auto pTexture = std::make_unique<TextureT<T_GFXAPI>>();
    *pTexture = std::move( CreateTextureFromBuffer( static_cast<T_GFXAPI&>( gfxApi ), pData, DataSize, Width, Height, Depth, Format, SamplerMode, Filter, pName ) );
    return pTexture;
}

/// Release a texture.
/// Must be specialized for the graphics api - will give linker error if called by application code.
template<typename T_GFXAPI>
void ReleaseTexture(T_GFXAPI& gfxApi, TextureT<T_GFXAPI>*);

/// Create a texture sampler (with some commonly used parameters)
template<typename T_GFXAPI>
SamplerT<T_GFXAPI> CreateSampler( T_GFXAPI& gfxApi, SamplerAddressMode SamplerMode, SamplerFilter FilterMode, SamplerBorderColor BorderColor, float MipBias )
{
    CreateSamplerObjectInfo createInfo{};
    createInfo.Mode = SamplerMode;
    createInfo.Filter = FilterMode;
    createInfo.BorderColor = BorderColor;
    createInfo.MipBias = MipBias;
    return CreateSampler( gfxApi, createInfo );
}

/// Template specialization for Vulkan CreateSampler
/// Create a texture sampler (must be specialized for the graphics api)
/// Must be specialized for the graphics api - will give linker error if called by application code.
template<typename T_GFXAPI>
SamplerT<T_GFXAPI> CreateSampler( T_GFXAPI& gfxApi, const CreateSamplerObjectInfo&)
{
    assert( 0 && "Expecting CreateSampler (per graphics api) to be used" );
    return {};
}

/// Release a texture sampler.
/// Must be specialized for the graphics api - will give linker error if called by application code.
template<typename T_GFXAPI>
void ReleaseSampler(T_GFXAPI& gfxApi, SamplerT<T_GFXAPI>*);
