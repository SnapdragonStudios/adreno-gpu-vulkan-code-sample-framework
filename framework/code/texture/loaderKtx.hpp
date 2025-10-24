//============================================================================================================
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>

///
/// KTX image file loading
/// 

// Forward declarations
struct ktxTexture;
class AssetManager;
template<typename T_GFXAPI> class Texture;
template<typename T_GFXAPI> class TextureKtx;
template<typename T_GFXAPI> class Sampler;

/// @brief opaque class holding texture data (in some texture format internal format, not a vulkan texture)
class TextureKtxFileWrapper final
{
    TextureKtxFileWrapper(const TextureKtxFileWrapper&) = delete;
    TextureKtxFileWrapper& operator=(const TextureKtxFileWrapper&) = delete;
public:
    TextureKtxFileWrapper() noexcept = default;
    TextureKtxFileWrapper(std::vector<uint8_t>&& fileData) noexcept : m_fileData(std::move(fileData)) {}
    ~TextureKtxFileWrapper() noexcept;
    TextureKtxFileWrapper(TextureKtxFileWrapper&&) noexcept;
    TextureKtxFileWrapper& operator=(TextureKtxFileWrapper&&) noexcept;
    operator bool() const { return m_ktxTexture != nullptr; }
    void Release();
protected:
    friend class TextureKtxBase;
    auto GetKtxTexture() const { return m_ktxTexture; }
private:
    ktxTexture* m_ktxTexture = nullptr;
    std::vector<uint8_t> m_fileData;            ///< contents of .ktx file (ktxTexture_CreateFromMemory does not take a copy of all the data in here, so we need to retain it)
};

/// @brief Class to handle loading KTX textures
/// Generally applications will want a singleton of this class (or a derived version eg @TextureKtxVulkan).
class TextureKtxBase
{
    TextureKtxBase(const TextureKtxBase&) = delete;
    TextureKtxBase& operator=(const TextureKtxBase&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = TextureKtx<T_GFXAPI>; // make apiCast work!
    TextureKtxBase() = default;
    virtual ~TextureKtxBase() noexcept;

    /// @brief Load the given ktx(2) texture 
    TextureKtxFileWrapper LoadFile(AssetManager& assetManager, const char* const pFileName) const;

    /// @brief Initialize this class prior to first use (derived classes must implement)
    /// @return true on success
    virtual bool Initialize() = 0;

    /// @brief Release (de-initialize) back to a clean (initializable) state
    virtual void Release() {}

    /// @brief Do the necessary upload etc to go from a cpu texture representation to Vulkan format
    /// WE EXPECT THERE TO BE A SPECIALIZED IMPLEMENTATION OF THIS TEMPLATE for each supported T_GFXAPI.
    /// @param textureFile ktx file data we want to load as a vulkan texture
    /// @param sampler sampler that loaded texture will take OWNERSHIP of
    /// @returns a &TextureVulkan, will be empty on failure
    template<typename T_GFXAPI>
    Texture<T_GFXAPI> LoadKtx(T_GFXAPI& gfxapi, const TextureKtxFileWrapper& textureFile, Sampler<T_GFXAPI> sampler)
    {
        return apiCast<T_GFXAPI>(this)->LoadKtx(gfxapi, textureFile, std::move(sampler));
    }

    /// @brief Load a ktx file and do the necessary upload etc to go from a cpu texture representation to Vulkan format
    /// WE EXPECT THERE TO BE A SPECIALIZED IMPLEMENTATION OF THIS TEMPLATE for each supported T_GFXAPI.
    /// @param filename of ktx (or ktx2) format file we want to load as a vulkan texture
    /// @param sampler sampler that loaded texture will take OWNERSHIP of
    /// @returns a &TextureVulkan, will be empty on failure
    template<typename T_GFXAPI>
    Texture<T_GFXAPI> LoadKtx(T_GFXAPI& gfxapi, AssetManager& assetManager, const char* const pFileName, Sampler<T_GFXAPI> sampler)
    {
        return apiCast<T_GFXAPI>(this)->LoadKtx(gfxapi, assetManager, pFileName, std::move(sampler));
    }

protected:
    /// Helper to get the ktx texture pointer (for derived classes)
    auto GetKtxTexture(const TextureKtxFileWrapper& tex) const { return tex.GetKtxTexture(); }
};

/// @brief Templated (by graphics api) ktx loader class, expected to be specialized.
/// @tparam T_GFXAPI 
template<typename T_GFXAPI>
class TextureKtx : public TextureKtxBase
{
    TextureKtx(const TextureKtx<T_GFXAPI>&) = delete;
    TextureKtx& operator=(const TextureKtx<T_GFXAPI>&) = delete;
public:
    TextureKtx(T_GFXAPI& rGfxApi) noexcept = delete;   // class expected to be specialized
    ~TextureKtx() noexcept override = delete;          // class expected to be specialized

    static_assert( sizeof( TextureKtx<T_GFXAPI> ) != sizeof( TextureKtxBase ) );   // Ensure this class template is specialized (and not used as-is)
};
