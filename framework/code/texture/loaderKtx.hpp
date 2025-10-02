//============================================================================================================
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
template<typename T_GFXAPI> class TextureT;
template<typename T_GFXAPI> class TextureKtxT;
template<typename T_GFXAPI> class SamplerT;

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
    friend class TextureKtx;
    auto GetKtxTexture() const { return m_ktxTexture; }
private:
    ktxTexture* m_ktxTexture = nullptr;
    std::vector<uint8_t> m_fileData;            ///< contents of .ktx file (ktxTexture_CreateFromMemory does not take a copy of all the data in here, so we need to retain it)
};

/// @brief Class to handle loading KTX textures
/// Generally applications will want a singleton of this class (or a derived version eg @TextureKtxVulkan).
class TextureKtx
{
    TextureKtx(const TextureKtx&) = delete;
    TextureKtx& operator=(const TextureKtx&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = TextureKtxT<T_GFXAPI>; // make apiCast work!
    TextureKtx() = default;
    virtual ~TextureKtx() noexcept;

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
    TextureT<T_GFXAPI> LoadKtx(T_GFXAPI& vulkan, const TextureKtxFileWrapper& textureFile, SamplerT<T_GFXAPI> sampler);

    /// @brief Load a ktx file and do the necessary upload etc to go from a cpu texture representation to Vulkan format
    /// WE EXPECT THERE TO BE A SPECIALIZED IMPLEMENTATION OF THIS TEMPLATE for each supported T_GFXAPI.
    /// @param filename of ktx (or ktx2) format file we want to load as a vulkan texture
    /// @param sampler sampler that loaded texture will take OWNERSHIP of
    /// @returns a &TextureVulkan, will be empty on failure
    template<typename T_GFXAPI>
    TextureT<T_GFXAPI> LoadKtx(T_GFXAPI& vulkan, AssetManager& assetManager, const char* const pFileName, SamplerT<T_GFXAPI> sampler);

protected:
    /// Helper to get the ktx texture pointer (for derived classes)
    auto GetKtxTexture(const TextureKtxFileWrapper& tex) const { return tex.GetKtxTexture(); }
};

/// @brief Templated (by graphics api) ktx loader class, expected to be specialized.
/// @tparam T_GFXAPI 
template<typename T_GFXAPI>
class TextureKtxT : public TextureKtx
{
    TextureKtxT(const TextureKtxT<T_GFXAPI>&) = delete;
    TextureKtxT& operator=(const TextureKtxT<T_GFXAPI>&) = delete;
public:
    TextureKtxT(T_GFXAPI& rGfxApi) noexcept = delete;   // class expected to be specialized
    ~TextureKtxT() noexcept override = delete;          // class expected to be specialized

    static_assert( sizeof( TextureKtxT<T_GFXAPI> ) != sizeof( TextureKtx ) );   // Ensure this class template is specialized (and not used as-is)
};
