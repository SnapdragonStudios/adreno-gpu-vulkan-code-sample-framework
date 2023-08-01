//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "../textureManager.hpp"
#include <map>

// Forward declarations
class Vulkan;

template<typename T_GFXAPI> class LoaderKtxT;
template<typename T_GFXAPI> class TextureT;
class TextureKtx;
template<typename T_GFXAPI> class TextureKtxT;
template<typename T_GFXAPI> class TexturePpmT;
template<typename T_GFXAPI> class TextureManagerT;
template<typename T_GFXAPI> class SamplerT;
using TextureManagerVulkan = TextureManagerT<Vulkan>;


/// @brief Templated implementation of TextureManager that handles ktx file loading for Vulkan graphics api.
/// @tparam T_GFXAPI 
template<>
class TextureManagerT<Vulkan> final : public TextureManager
{
public:
    using tGfxApi = Vulkan;

    TextureManagerT(tGfxApi& rGfxApi) noexcept;
    virtual ~TextureManagerT() override;

    bool Initialize();
    void Release() override;

    /// @brief Helper to return the ktx file loader directly.  If this throws a compile error then .cpp that includes this file is missing #include "texture/[gfxapi]/loaderKtx.hpp"
    TextureKtx* GetLoader() const { return m_Loader.get(); }
    TexturePpm* GetLoaderPpm() const { return m_LoaderPpm.get(); }

    const Texture* GetTexture(const std::string& textureSlotName) const override;

    /// Create texture (generally for render target usage).  Uses CreateTexObjectInfo structure to define texture creation parameters.
    /// Implements the base class virtual function.
    std::unique_ptr<Texture> CreateTextureObject(GraphicsApiBase& gfxApi, const CreateTexObjectInfo& texInfo) override;

    /// Create texture from a block of texture data in memory (with correct format, span etc).
    /// Implements the base class virtual function.
    std::unique_ptr<Texture> CreateTextureFromBuffer( GraphicsApiBase&, const void* pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, TextureFormat Format, SamplerAddressMode SamplerMode, SamplerFilter Filter, const char* pName ) override;

    /// Create a texture that views (aliases) another texture but using a different texture format (must be 'related' formats, which formats are related is dependant on graphics api)
    /// Implements the base class virtual function.
    std::unique_ptr<Texture> CreateTextureObjectView( GraphicsApiBase& gfxApi, const Texture& original, TextureFormat viewFormat ) override;

    /// Get a 'default' sampler for the given address mode (all other sampler settings assumed to be 'normal' ie linearly sampled etc)
    const Sampler* const GetSampler( SamplerAddressMode ) const override;

protected:
    const Texture* GetOrLoadTexture_(const std::string& textureSlotName, AssetManager& rAssetManager, const std::string& filename, const Sampler& sampler) override;
    void BatchLoad(AssetManager& rAssetManager, const std::span<std::pair<std::string, std::string>>, const Sampler& defaultSampler) override;

private:
    std::map<std::string, TextureT<tGfxApi>> m_LoadedTextures;
    std::vector<SamplerT<Vulkan>>            m_DefaultSamplers;
    const bool                               m_MirrorClampToEdgeSupported = false;  // currently this is never set, if we add VK_KHR_sampler_mirror_clamp_to_edge extension support or samplerMirrorClampToEdge feature flag then we can change this to non const and set/reset
    tGfxApi&                                 m_GfxApi;
};
