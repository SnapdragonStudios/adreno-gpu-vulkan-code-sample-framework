//============================================================================================================
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include "texture.hpp"

///
/// PPM image file loading
/// 

// Forward declarations
class AssetManager;

/// @brief opaque class holding texture data (in some texture format internal format, not a vulkan texture)
class TexturePpmFileWrapper final
{
    TexturePpmFileWrapper(const TexturePpmFileWrapper&) = delete;
    TexturePpmFileWrapper& operator=(const TexturePpmFileWrapper&) = delete;
public:
    TexturePpmFileWrapper() noexcept = default;
    ~TexturePpmFileWrapper() noexcept = default;
    TexturePpmFileWrapper( uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, std::vector<uint8_t>&& data ) noexcept
        : m_Width( width ), m_Height( height ), m_Depth( depth ), m_Format( format ), m_Data( std::move( data ) )
    {}
    TexturePpmFileWrapper(TexturePpmFileWrapper&&) noexcept;
    TexturePpmFileWrapper& operator=(TexturePpmFileWrapper&&) noexcept;
    operator bool() const { return !m_Data.empty(); }
    void Release();
protected:
    friend class TexturePpmBase;
private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_Depth = 0;
    TextureFormat m_Format = TextureFormat::UNDEFINED;
    std::vector<uint8_t> m_Data;    // processed data (in m_Format and m_Width * m_Height * m_Depth number of pixels)
};

/// @brief Class to handle loading PPM textures
/// Generally applications will want a singleton of this class.
class TexturePpmBase
{
    TexturePpmBase(const TexturePpmBase&) = delete;
    TexturePpmBase& operator=(const TexturePpmBase&) = delete;
public:
    TexturePpmBase() = default;
    virtual ~TexturePpmBase() noexcept;

    /// @brief Load the given ppm(2) texture 
    TexturePpmFileWrapper LoadFile(AssetManager& assetManager, const char* const pFileName) const;

    /// @brief Initialize this class prior to first use (derived classes must implement)
    /// @return true on success
    virtual bool Initialize() = 0;

    /// @brief Release (de-initialize) back to a clean (initializable) state
    virtual void Release() {}

    template<typename T_GFXAPI>
    Texture<T_GFXAPI> LoadPpm( T_GFXAPI& gfxApi, AssetManager& assetManager, const char* const pFileName, const Sampler<T_GFXAPI>& sampler );
    template<typename T_GFXAPI>
    Texture<T_GFXAPI> LoadPpm( T_GFXAPI& gfxApi, const TexturePpmFileWrapper& ppmData, const Sampler<T_GFXAPI>& sampler );
};


template<typename T_GFXAPI>
Texture<T_GFXAPI> TexturePpmBase::LoadPpm( T_GFXAPI& gfxApi, AssetManager& assetManager, const char* const pFileName, const Sampler<T_GFXAPI>& sampler )
{
    auto ppmData = LoadFile( assetManager, pFileName );
    if (!ppmData)
        return {};
    return LoadPpm( gfxApi, ppmData, sampler );
}

template<typename T_GFXAPI>
Texture<T_GFXAPI> TexturePpmBase::LoadPpm( T_GFXAPI& gfxApi, const TexturePpmFileWrapper& ppmData, const Sampler<T_GFXAPI>& sampler )
{
    return ::CreateTextureFromBuffer( gfxApi, ppmData.m_Data.data(), ppmData.m_Data.size(), ppmData.m_Width, ppmData.m_Height, ppmData.m_Depth, ppmData.m_Format, SamplerAddressMode::Repeat, SamplerFilter::Linear );
}


/// @brief Templated (by graphics api) ppm loader class, expected to be specialized.
/// @tparam T_GFXAPI 
template<typename T_GFXAPI>
class TexturePpm : public TexturePpmBase
{
    TexturePpm( const TexturePpm<T_GFXAPI>& ) = delete;
    TexturePpm& operator=( const TexturePpm<T_GFXAPI>& ) = delete;
public:
    TexturePpm( T_GFXAPI& rGfxApi ) noexcept = delete;   // class expected to be specialized
    ~TexturePpm() noexcept override = delete;          // class expected to be specialized

    static_assert( sizeof( TexturePpm<T_GFXAPI> ) != sizeof( TexturePpm ) );   // Ensure this class template is specialized (and not used as-is)
};
