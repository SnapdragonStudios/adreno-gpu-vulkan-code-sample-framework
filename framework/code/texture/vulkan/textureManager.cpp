//=============================================================================
//
//                  Copyright (c) 2023 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#include "textureManager.hpp"
#include "texture.hpp"
#include "imageWrapper.hpp"
#include "loaderKtx.hpp"
#include "../loaderPpm.hpp"
#include "vulkan/vulkan.hpp"
#include "memory/memory.hpp"


//-----------------------------------------------------------------------------
TextureManagerT<Vulkan>::TextureManagerT( tGfxApi& rGfxApi ) noexcept : TextureManager(), m_GfxApi( rGfxApi )
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
TextureManagerT<Vulkan>::~TextureManagerT()
//-----------------------------------------------------------------------------
{
    Release();
}

//-----------------------------------------------------------------------------
bool TextureManagerT<Vulkan>::Initialize()
//-----------------------------------------------------------------------------
{
    m_Loader = std::make_unique<TextureKtxT<tGfxApi>>( static_cast<tGfxApi&>(m_GfxApi) );

    m_DefaultSamplers.reserve( size_t(SamplerAddressMode::End) );
    for (size_t i = (size_t)SamplerAddressMode::Repeat; i < (size_t)SamplerAddressMode::End; ++i)
    {
        SamplerAddressMode samplerMode = (SamplerAddressMode)i;
        if (samplerMode != SamplerAddressMode::MirroredClampEdge || m_MirrorClampToEdgeSupported)
            m_DefaultSamplers.push_back( CreateSampler( m_GfxApi, samplerMode, SamplerFilter::Linear, SamplerBorderColor::TransparentBlackFloat, 0.0f ) );
        else
            m_DefaultSamplers.push_back( {} );
    }

    return TextureManager::Initialize();
}

//-----------------------------------------------------------------------------
void TextureManagerT<Vulkan>::Release()
//-----------------------------------------------------------------------------
{
    for (auto& [key, texture] : m_LoadedTextures)
    {
        texture.Release(&m_GfxApi);
    }
    m_LoadedTextures.clear();
    m_Loader->Release();
    TextureManager::Release();
}

//-----------------------------------------------------------------------------
const Texture* TextureManagerT<Vulkan>::GetTexture(const std::string& textureSlotName) const
//-----------------------------------------------------------------------------
{
    auto iter = m_LoadedTextures.find(textureSlotName);
    if (iter == m_LoadedTextures.end())
        return nullptr;
    return &iter->second;
}

//-----------------------------------------------------------------------------
const Sampler* const TextureManagerT<Vulkan>::GetSampler( SamplerAddressMode sam ) const
//-----------------------------------------------------------------------------
{
    return &m_DefaultSamplers[size_t(sam)];
}

//-----------------------------------------------------------------------------
const Texture* TextureManagerT<Vulkan>::GetOrLoadTexture_(const std::string& textureSlotName, AssetManager& rAssetManager, const std::string& filename, const Sampler& sampler)
//-----------------------------------------------------------------------------
{
    const Texture* pTexture = GetTexture(textureSlotName);
    if (!pTexture)
    {
        const SamplerVulkan& samplerVulkan = static_cast<const SamplerVulkan&>(sampler);
        auto loadedTexture = GetLoader()->LoadKtx(m_GfxApi, rAssetManager, filename.c_str(), std::move(samplerVulkan.Copy()));
        if (!loadedTexture.IsEmpty())
        {
            auto insertedIt = m_LoadedTextures.insert({ textureSlotName, std::move(loadedTexture) });
            pTexture = &(insertedIt.first->second);
        }
    }

    return pTexture;
}

typedef std::queue<std::pair<size_t, TextureKtxFileWrapper>> tLoadedFileQueue;
struct BatchLoadThreadParams {
    AssetManager& assetManager;
    std::mutex& loadedFileQueueMutex;
    tLoadedFileQueue& loadedFileQueue;
    Semaphore& dataReadySema;
    const std::string& filename;
    size_t slotIndex;;
};

//-----------------------------------------------------------------------------
void TextureManagerT<Vulkan>::BatchLoad(AssetManager& rAssetManager, const std::span<std::pair<std::string/*textureSlotName*/, std::string/*filename*/>> slotAndFileNames, const Sampler& defaultSampler)
//-----------------------------------------------------------------------------
{
    std::mutex loadedFileQueueMutex;
    tLoadedFileQueue loadedFileQueue;
    Semaphore dataReadySema{ 0 };
    Semaphore finishedSema{ 0 };

    // Setup the output textures
    std::vector<TextureT<Vulkan>> vulkanTextures;
    vulkanTextures.resize(slotAndFileNames.size());

    // We have one worker job just grabbing loaded textures and transfering them to vulkan (gpu memory).
    struct TransferWorkerParams {
        std::mutex& loadedFileQueueMutex;
        tLoadedFileQueue& loadedFileQueue;
        Semaphore& dataReadySema;
        Semaphore& finishedSema;
        const SamplerVulkan& defaultSampler;
        std::vector<TextureT<Vulkan>>& vulkanTextures;
    } transferWorkerParams{ loadedFileQueueMutex, loadedFileQueue, dataReadySema, finishedSema, apiCast<Vulkan>(defaultSampler), vulkanTextures };

    m_LoadingThreadWorker.DoWork2([](TextureManagerVulkan* pThis, TransferWorkerParams params)
    {
        size_t texturesRemaining = params.vulkanTextures.size();
        while (texturesRemaining > 0)
        {
            TextureKtxFileWrapper ktxData;
            size_t slotIndex;
            params.dataReadySema.Wait();
            {
                std::lock_guard<std::mutex> lock(params.loadedFileQueueMutex);
                auto&& loadedData = params.loadedFileQueue.front();
                ktxData = std::move(loadedData.second);
                slotIndex = loadedData.first;
                params.loadedFileQueue.pop();
            }
            if (ktxData)
                params.vulkanTextures[slotIndex] = pThis->GetLoader()->LoadKtx(pThis->m_GfxApi, ktxData, std::move(params.defaultSampler.Copy()) );
            --texturesRemaining;
        }
        params.finishedSema.Post();

    }, this, transferWorkerParams);

    size_t currentSlotIndex = 0;
    for (const auto& [textureSlotName, filename] : slotAndFileNames)
    {
        auto iter = m_LoadedTextures.find(textureSlotName);
        if (iter == m_LoadedTextures.end())
        {
            BatchLoadThreadParams params{ rAssetManager, loadedFileQueueMutex, loadedFileQueue, dataReadySema, filename, currentSlotIndex };

            m_LoadingThreadWorker.DoWork2([](TextureManagerVulkan* pThis, BatchLoadThreadParams params)
            {
                auto ktxData = pThis->m_Loader->LoadFile(params.assetManager, params.filename.c_str());
                auto* pKtxLoader = static_cast<TextureKtxT<Vulkan>*>(pThis->GetLoader());
                ktxData = pKtxLoader->Transcode(std::move(ktxData));
                {
                    std::lock_guard<std::mutex> lock(params.loadedFileQueueMutex);
                    params.loadedFileQueue.emplace(std::pair{ params.slotIndex, std::move(ktxData) });
                }
                params.dataReadySema.Post();
            }, this, params);

            ++currentSlotIndex;
        }
        else
        {
            // Signal the transfer thread, but dont give it any data (it finishes when it has recieved a signal for every texture being BatchLoaded)
            dataReadySema.Post();
        }
    }

    finishedSema.Wait();

    // Transfer all the loaded textures to m_LoadedTextures 
    for (size_t i = 0; i < vulkanTextures.size(); ++i)
    {
        if (!vulkanTextures[i].IsEmpty())
        {
            m_LoadedTextures.emplace(std::pair<std::string, TextureT<Vulkan>>{ slotAndFileNames[i].first/*slot*/, std::move(vulkanTextures[i])});
        }
    }
}

//-----------------------------------------------------------------------------
std::unique_ptr<Texture> TextureManagerT<Vulkan>::CreateTextureObject(GraphicsApiBase& gfxApi, const CreateTexObjectInfo& texInfo) /*override*/
//-----------------------------------------------------------------------------
{
    auto pTexture = std::make_unique<TextureT<Vulkan>>();
    *pTexture = ::CreateTextureObject(static_cast<Vulkan&>(gfxApi), texInfo);
    return pTexture;
}

//-----------------------------------------------------------------------------
std::unique_ptr<Texture> TextureManagerT<Vulkan>::CreateTextureFromBuffer( GraphicsApiBase& gfxApi, const void* pData, size_t DataSize, uint32_t Width, uint32_t Height, uint32_t Depth, TextureFormat Format, SamplerAddressMode SamplerMode, SamplerFilter Filter, const char* pName )
//-----------------------------------------------------------------------------
{
    auto pTexture = std::make_unique<TextureT<Vulkan>>();
    *pTexture = ::CreateTextureFromBuffer( static_cast<Vulkan&>( gfxApi ), pData, DataSize, Width, Height, Depth, Format, SamplerMode, Filter, pName );
    return pTexture;
}

//-----------------------------------------------------------------------------
std::unique_ptr<Texture> TextureManagerT<Vulkan>::CreateTextureObjectView( GraphicsApiBase& gfxApi, const Texture& original, TextureFormat viewFormat )
//-----------------------------------------------------------------------------
{
    auto pTexture = std::make_unique<TextureT<Vulkan>>();
    *pTexture = ::CreateTextureObjectView( static_cast<Vulkan&>( gfxApi ), apiCast<Vulkan>(original), viewFormat );
    return pTexture;
}
