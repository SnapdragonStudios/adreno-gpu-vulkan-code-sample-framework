//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include <span>
#include <string>
#include <functional>

///
/// Texture file loading and tracking
/// 

#include "system/Worker.h"
#include "texture.hpp"

// Forward declarations
class AssetManager;
class TextureKtx;
class TexturePpm;
class Texture;
class Sampler;
class GraphicsApiBase;
template<typename T_GFXAPI> class TextureManagerT;


class TextureManager
{
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	template<typename T, typename... TT>
	static auto ExecutePathManipulators(std::string& filename, T&& currentManipulator, TT && ... subsequentManipulators);
protected:
    TextureManager() noexcept;
    virtual void Release();
    bool Initialize();
public:
    virtual ~TextureManager();
    template<typename T_GFXAPI> using tApiDerived = TextureManagerT<T_GFXAPI>; // make apiCast work!

	/// @brief Load a texture if we haven't already loaded it (into the slot 'textureName'), otherwise return the previously loaded texture
	/// @param rAssetManager asset manager to use to load the file
	/// @param textureName name of texture (key in lookup)
	/// @param ...pathnameManipulators variadic functors to manipulate the textureName into a loadable image filename (eg change extension or path)
	/// @return pointer to loaded texture info
	template<typename T_SAMPLER, typename... T_PATHMANIPULATOR>
	const Texture* GetOrLoadTexture(AssetManager& rAssetManager, const std::string& textureName, const T_SAMPLER& sampler, T_PATHMANIPULATOR && ... pathnameManipulators);

	/// @brief Load a texture if we haven't already loaded it into a named 'slot', otherwise return the previously loaded texture
	/// @param rAssetManager asset manager to use to load the file
	/// @param textureName name of texture (key in lookup)
	/// @param ...pathnameManipulators variadic functors to manipulate the textureName into a loadable image filename (eg change extension or path)
	/// @return pointer to loaded texture info
	template<typename T_SAMPLER, typename... T_PATHMANIPULATOR>
	const Texture* GetOrLoadTexture(const std::string& textureSlotName, AssetManager& rAssetManager, const std::string& filename, const T_SAMPLER& sampler, T_PATHMANIPULATOR && ... pathnameManipulators);

	template<typename... T_PATHMANIPULATOR>
	void BatchLoad(AssetManager& rAssetManager, const std::span<const std::string> textureNames, const Sampler& defaultSampler, T_PATHMANIPULATOR && ... pathnameManipulators);

	/// @brief Find a texture (by slot name) that may be already loaded
	/// @param textureSlotName name to look for
	/// @return pointer to already loaded texture, or null
	virtual const Texture* GetTexture(const std::string& textureSlotName) const = 0;

	/// @brief Set the pathname manipulators used by GetOrLoadTexture if it is not supplied a pathnameManipulators parameter.
	/// @param ...pathnameManipulators variadic functors to manipulate the textureName into a loadable image filename (eg change extension or path)
	template<class ... T_PATHMANIPULATOR>
	void SetDefaultFilenameManipulators(T_PATHMANIPULATOR && ... pathnameManipulators);

    /// Create texture (generally for render target usage)
    std::unique_ptr<Texture> CreateTextureObject(GraphicsApiBase&, uint32_t uiWidth, uint32_t uiHeight, TextureFormat Format, TEXTURE_TYPE TexType, const char* pName, uint32_t Msaa = 1, TEXTURE_FLAGS Flags = TEXTURE_FLAGS::None);

    /// Create texture (generally for render target usage).  Uses CreateTexObjectInfo structure to define texture creation parameters.  Must be implemented per graphics api
    virtual std::unique_ptr<Texture> CreateTextureObject(GraphicsApiBase&, const CreateTexObjectInfo& texInfo) = 0;

    /// Create a texture that views (aliases) another texture but using a different texture format (must be 'related' formats, which formats are related is dependant on graphics api)
    virtual std::unique_ptr<Texture> CreateTextureObjectView( GraphicsApiBase& gfxApi, const Texture& original, TextureFormat viewFormat ) = 0;

    /// Create texture from a block of texture data in memory (with correct format, span etc).
    virtual std::unique_ptr<Texture> CreateTextureFromBuffer( GraphicsApiBase&, const void* pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, TextureFormat Format, SamplerAddressMode SamplerMode, SamplerFilter Filter, const char* pName, uint32_t extraFlags = 0) = 0;

    /// Get a 'default' sampler for the given address mode (all other sampler settings assumed to be 'normal' ie linearly sampled etc)
    virtual const Sampler* const GetSampler( SamplerAddressMode ) const = 0;

protected:
    const Texture* GetOrLoadTexture_( const std::string& textureSlotName, AssetManager& rAssetManager, const std::string& filename, const SamplerAddressMode& sampler )
    {
        return GetOrLoadTexture_( textureSlotName, rAssetManager, filename, *GetSampler( sampler ) );
    }

    virtual const Texture* GetOrLoadTexture_( const std::string& textureSlotName, AssetManager& rAssetManager, const std::string& filename, const Sampler& sampler ) = 0;
    virtual void BatchLoad(AssetManager& rAssetManager, const std::span<std::pair<std::string, std::string>>, const Sampler& defaultSampler) = 0;

protected:
    std::unique_ptr<TextureKtx>		        m_Loader;
    std::unique_ptr<TexturePpm>		        m_LoaderPpm;
    std::function<void(std::string&)>       m_DefaultFilenameManipulator = [](std::string&) {return; };
	CWorker									m_LoadingThreadWorker;
};


/// @brief Templated TextureManager (for graphics api specific implementations)
/// This template is expected to be specialized.
/// @tparam T_GFXAPI
template<typename T_GFXAPI>
class TextureManagerT final : public TextureManager
{
    TextureManagerT() noexcept = delete;     // Error if we use this non specialized version of the TextureManagerT.
    ~TextureManagerT() override = delete;    // Error if we use this non specialized version of the TextureManagerT.
};


/// @brief Helper class/functor to prefix filename with a (given) directory string
/// Can be passed as a parameter to TextureManager::SetDefaultFilenameManipulators or TextureManager::GetOrLoadTexture
struct PathManipulator_PrefixDirectory
{
	/// @param prefix to change to put before filename (eg "Textures/")
	PathManipulator_PrefixDirectory(std::string prefix) noexcept : m_prefix(std::move(prefix)) {};
	void operator()(std::string& filename) const noexcept { filename.assign(m_prefix + filename); };
	std::string m_prefix;
};

/// @brief Helper class/functor to change filename extension
/// Can be passed as a parameter to TextureManager::SetDefaultFilenameManipulators or TextureManager::GetOrLoadTexture
struct PathManipulator_ChangeExtension
{
	/// @param extension to change to (eg ".ktx")
	PathManipulator_ChangeExtension(std::string extension) noexcept : m_newExtension(std::move(extension)) { }
	void operator()(std::string& filename) const {
		auto extPos = filename.rfind('.');
		if (extPos != std::string::npos)
			filename.replace(filename.begin() + extPos, filename.end(), m_newExtension);
	};
	std::string m_newExtension;
};

//
// Template implementations
//
template<typename T, typename... TT>
auto TextureManager::ExecutePathManipulators(std::string& filename, T&& currentManipulator, TT && ... subsequentManipulators)
{
	currentManipulator(filename);
	if constexpr (sizeof...(subsequentManipulators) == 0)
		return;																	// no more manipulators to execute.  success!
	else
		ExecutePathManipulators(filename, subsequentManipulators...);// Recursively call the subsequent manipulator(s).
}

template<typename T_SAMPLER, typename... T_PATHMANIPULATOR>
const Texture* TextureManager::GetOrLoadTexture(AssetManager& rAssetManager, const std::string& textureName, const T_SAMPLER& sampler, T_PATHMANIPULATOR && ... pathnameManipulators)
{
	if (textureName.empty())
		return nullptr;
    auto* pTexture = GetTexture(textureName);
    if (pTexture)
        return pTexture;

	std::string filename = textureName;
	if constexpr (sizeof...(pathnameManipulators) == 0)
		m_DefaultFilenameManipulator(filename);
	else
		ExecutePathManipulators(filename, pathnameManipulators...);
	return GetOrLoadTexture_(textureName, rAssetManager, filename, sampler);
}

template<typename T_SAMPLER, typename... T_PATHMANIPULATOR>
const Texture* TextureManager::GetOrLoadTexture(const std::string& textureSlotName, AssetManager& rAssetManager, const std::string& filename, const T_SAMPLER& sampler, T_PATHMANIPULATOR && ... pathnameManipulators)
{
    if (textureSlotName.empty())
        return nullptr;
    auto* pTexture = GetTexture(textureSlotName);
    if (pTexture)
        return pTexture;

    if (filename.empty())
        return nullptr;
    std::string filename_ = filename;
    if constexpr (sizeof...(pathnameManipulators) == 0)
        m_DefaultFilenameManipulator(filename_);
    else
        ExecutePathManipulators(filename_, pathnameManipulators...);
    return GetOrLoadTexture_(textureSlotName, rAssetManager, filename_, sampler);
}

template<typename... T_PATHMANIPULATOR>
void TextureManager::BatchLoad(AssetManager& rAssetManager, const std::span<const std::string> textureNames, const Sampler& defaultSampler, T_PATHMANIPULATOR && ... pathnameManipulators)
{
	std::vector< std::pair<std::string, std::string>> filenamesAndSlotNames;
	filenamesAndSlotNames.reserve(textureNames.size());
	for (std::string textureName : textureNames)
	{
		std::string filename = textureName;
		if constexpr (sizeof...(pathnameManipulators) == 0)
			m_DefaultFilenameManipulator(filename);
		else
			ExecutePathManipulators(filename, pathnameManipulators...);
		filenamesAndSlotNames.emplace_back(std::move(textureName), std::move(filename));
	}
	BatchLoad(rAssetManager, filenamesAndSlotNames, defaultSampler);
}

template<class ... T_PATHMANIPULATOR>
void TextureManager::SetDefaultFilenameManipulators(T_PATHMANIPULATOR && ... pathnameManipulators)
{
	m_DefaultFilenameManipulator = [... p = std::forward<T_PATHMANIPULATOR>(pathnameManipulators)](std::string& filename) -> void {
		ExecutePathManipulators(filename, p ...);
	};
}
