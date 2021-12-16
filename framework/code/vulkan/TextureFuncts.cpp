// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

// TextureFuncts.cpp
//      Vulkan texture handling support

#include "system/os_common.h"
#include "system/assetManager.hpp"
#include "memory/memoryManager.hpp"
#include "vulkan_support.hpp"
#include "TextureFuncts.h"
#include <vector>
#include <map>

// http://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
constexpr std::array<unsigned char, 12> KTX_IDENTIFIER_REF = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
constexpr uint32_t KTX_ENDIAN_REF = 0x04030201;     // Big Endian
constexpr uint32_t KTX_ENDIAN_REF_REV = 0x01020304; // Little Endian
constexpr uint32_t KTX_HEADER_SIZE = 64;

#if !defined(FLT_MAX)
#define FLT_MAX          3.402823466e+38f
#endif // !defined(FLT_MAX)


//-----------------------------------------------------------------------------
// Implementation data structures
//-----------------------------------------------------------------------------

typedef struct _KTXHeader
{
    uint8_t   identifier[12];
    uint32_t  endianness;
    uint32_t  glType;
    uint32_t  glTypeSize;
    uint32_t  glFormat;
    uint32_t  glInternalFormat;
    uint32_t  glBaseInternalFormat;
    uint32_t  pixelWidth;
    uint32_t  pixelHeight;
    uint32_t  pixelDepth;
    uint32_t  numberOfArrayElements;
    uint32_t  numberOfFaces;
    uint32_t  numberOfMipmapLevels;
    uint32_t  bytesOfKeyValueData;
} KTXHeader;

// This is not a class.  It is up to caller to release any objects in this structure
typedef struct _MipTexData
{
    uint32_t        Width;
    uint32_t        Height;
    uint32_t        Size;
    void*           pData;
} MipTexData;

typedef struct _LayerTexData
{
    uint32_t        NumMipLevels;
    MipTexData*     pMipData;
} LayerTexData;

typedef struct _FaceTexData
{
    uint32_t        NumLayers;
    LayerTexData*   pLayerData;
} FaceTexData;

typedef struct _VulkanTexData
{
    uint32_t        glType;
    uint32_t        glFormat;
    uint32_t        glInternalFormat;
    VkFormat        VulkanFormat;

    uint32_t        NumFaces;
    FaceTexData*    pFaceData;
} VulkanTexData;


//-----------------------------------------------------------------------------
void L_FreeTexData(VulkanTexData* pTexData)
//-----------------------------------------------------------------------------
{
    if (pTexData == NULL)
        return;

    if (pTexData->NumFaces == 0 && pTexData->pFaceData == NULL)
    {
        // The data structure is already reset
        return;
    }

    // If any of these next conditions are true the structure is in a horked state!
    if (pTexData->NumFaces == 0 || pTexData->pFaceData == NULL)
    {
        LOGE("Texture data is horked: NumFaces is 0 or FaceData is NULL!");
        return;
    }

    if (pTexData->pFaceData->NumLayers == 0 || pTexData->pFaceData->pLayerData == NULL)
    {
        LOGE("Texture data is horked: NumLayers is 0 or LayerData is NULL!");
        return;
    }

    if (pTexData->pFaceData->pLayerData->NumMipLevels == 0 || pTexData->pFaceData->pLayerData->pMipData == NULL)
    {
        LOGE("Texture data is horked: NumMipLevels is 0 or MipData is NULL!");
        return;
    }

    // Each texture has faces...
    for (uint32_t WhichFace = 0; WhichFace < pTexData->NumFaces; WhichFace++)
    {
        // ...each face has layers...
        for (uint32_t WhichLayer = 0; WhichLayer < pTexData->pFaceData[WhichFace].NumLayers; WhichLayer++)
        {
            // ...each layer has mip levels
            for (uint32_t WhichMipLevel = 0; WhichMipLevel < pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].NumMipLevels; WhichMipLevel++)
            {
                free(pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMipLevel].pData);
                pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMipLevel].pData = NULL;
            }   // Which MipLevel

            free(pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData);
            pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].NumMipLevels = 0;
            pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData = NULL;
        }   // Which Layer

        free(pTexData->pFaceData[WhichFace].pLayerData);
        pTexData->pFaceData[WhichFace].NumLayers = 0;;
        pTexData->pFaceData[WhichFace].pLayerData = NULL;
    }   // Which Face

    free(pTexData->pFaceData);
    pTexData->NumFaces = 0;
    pTexData->pFaceData = NULL;
}

static const std::map<uint32_t, VkFormat> CompressedGlFormatToVkFormat = {
    {0x83F0/*GL_COMPRESSED_RGB_S3TC_DXT1_EXT*/, VK_FORMAT_BC1_RGB_UNORM_BLOCK},
    {0x83F1/*GL_COMPRESSED_RGBA_S3TC_DXT1_EXT*/, VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
    {0x83F2/*GL_COMPRESSED_RGBA_S3TC_DXT3_EXT*/, VK_FORMAT_BC2_UNORM_BLOCK},
    {0x83F3/*GL_COMPRESSED_RGBA_S3TC_DXT5_EXT*/, VK_FORMAT_BC3_UNORM_BLOCK},
    {0x8C4C/*GL_COMPRESSED_SRGB_S3TC_DXT1_EXT*/, VK_FORMAT_BC1_RGB_SRGB_BLOCK },
    {0x8C4D/*GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT*/, VK_FORMAT_BC1_RGBA_SRGB_BLOCK },
    {0x8C4E/*GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT*/, VK_FORMAT_BC2_SRGB_BLOCK},
    {0x8C4F/*GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT*/, VK_FORMAT_BC3_SRGB_BLOCK},

    {0x9274/*GL_COMPRESSED_RGB8_ETC2*/, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK},
    {0x9275/*GL_COMPRESSED_SRGB8_ETC2*/, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK},
    {0x9276/*GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2*/, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK },
    {0x9277/*GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2*/, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK },
    {0x9278/*GL_COMPRESSED_RGBA8_ETC2_EAC*/, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK},
    {0x9279/*GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC*/, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK},

    {0x93B0/*GL_COMPRESSED_RGBA_ASTC_4x4_KHR*/, VK_FORMAT_ASTC_4x4_UNORM_BLOCK},
    {0x93B1/*GL_COMPRESSED_RGBA_ASTC_5x4_KHR*/, VK_FORMAT_ASTC_5x4_UNORM_BLOCK},
    {0x93B2/*GL_COMPRESSED_RGBA_ASTC_5x5_KHR*/, VK_FORMAT_ASTC_5x5_UNORM_BLOCK},
    {0x93B3/*GL_COMPRESSED_RGBA_ASTC_6x5_KHR*/, VK_FORMAT_ASTC_6x5_UNORM_BLOCK},
    {0x93B4/*GL_COMPRESSED_RGBA_ASTC_6x6_KHR*/, VK_FORMAT_ASTC_6x6_UNORM_BLOCK},
    {0x93B5/*GL_COMPRESSED_RGBA_ASTC_8x5_KHR*/, VK_FORMAT_ASTC_8x5_UNORM_BLOCK},
    {0x93B6/*GL_COMPRESSED_RGBA_ASTC_8x6_KHR*/, VK_FORMAT_ASTC_8x6_UNORM_BLOCK},
    {0x93B7/*GL_COMPRESSED_RGBA_ASTC_8x8_KHR*/, VK_FORMAT_ASTC_8x8_UNORM_BLOCK},
    {0x93B8/*GL_COMPRESSED_RGBA_ASTC_10x5_KHR*/, VK_FORMAT_ASTC_10x5_UNORM_BLOCK},
    {0x93B9/*GL_COMPRESSED_RGBA_ASTC_10x6_KHR*/, VK_FORMAT_ASTC_10x6_UNORM_BLOCK},
    {0x93BA/*GL_COMPRESSED_RGBA_ASTC_10x8_KHR*/, VK_FORMAT_ASTC_10x8_UNORM_BLOCK},
    {0x93BB/*GL_COMPRESSED_RGBA_ASTC_10x10_KHR*/, VK_FORMAT_ASTC_10x10_UNORM_BLOCK},
    {0x93BC/*GL_COMPRESSED_RGBA_ASTC_12x10_KHR*/, VK_FORMAT_ASTC_12x10_UNORM_BLOCK},
    {0x93BD/*GL_COMPRESSED_RGBA_ASTC_12x12_KHR*/, VK_FORMAT_ASTC_12x12_UNORM_BLOCK},

    {0x93D0/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR*/, VK_FORMAT_ASTC_4x4_SRGB_BLOCK},
    {0x93D1/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR*/, VK_FORMAT_ASTC_5x4_SRGB_BLOCK},
    {0x93D2/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR*/, VK_FORMAT_ASTC_5x5_SRGB_BLOCK},
    {0x93D3/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR*/, VK_FORMAT_ASTC_6x5_SRGB_BLOCK},
    {0x93D4/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR*/, VK_FORMAT_ASTC_6x6_SRGB_BLOCK},
    {0x93D5/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR*/, VK_FORMAT_ASTC_8x5_SRGB_BLOCK},
    {0x93D6/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR*/, VK_FORMAT_ASTC_8x6_SRGB_BLOCK},
    {0x93D7/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR*/, VK_FORMAT_ASTC_8x8_SRGB_BLOCK},
    {0x93D8/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR*/, VK_FORMAT_ASTC_10x5_SRGB_BLOCK},
    {0x93D9/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR*/, VK_FORMAT_ASTC_10x6_SRGB_BLOCK},
    {0x93DA/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR*/, VK_FORMAT_ASTC_10x8_SRGB_BLOCK},
    {0x93DB/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR*/, VK_FORMAT_ASTC_10x10_SRGB_BLOCK},
    {0x93DC/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR*/, VK_FORMAT_ASTC_12x10_SRGB_BLOCK},
    {0x93DD/*GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR*/, VK_FORMAT_ASTC_12x12_SRGB_BLOCK}
};

//-----------------------------------------------------------------------------
VkFormat L_ReturnVulkanFormat(uint32_t glType, uint32_t glFormat, uint32_t glInternalFormat)
//-----------------------------------------------------------------------------
{
    // Really need GL versions of these numbers
    const uint32_t glType_GL_UNSIGNED_BYTE = 0x1401;
    const uint32_t glType_GL_HALF_FLOAT = 0x140B;
    const uint32_t glType_GL_FLOAT = 0x1406;

    const uint32_t glFormat_GL_RGB = 0x1907;
    const uint32_t glFormat_GL_RGBA = 0x1908;

    const uint32_t glFormat_GL_RGB8 = 0x8051;
    const uint32_t glFormat_GL_RGBA8 = 0x8058;

    const uint32_t glFormat_GL_SRGB8_EXT = 0x8C41;
    const uint32_t glFormat_GL_SRGB8_ALPHA8_EXT = 0x8C43;

    const uint32_t glFormat_GL_RGBA16F = 0x881A;
    const uint32_t glFormat_GL_RGBA32F = 0x8814;

    VkFormat RetVal = VK_FORMAT_UNDEFINED;

    // Unsigned Byte Formats (SimpleTextureConverted uses either GL_RGB or GL_RGB8)
    if (glType == glType_GL_UNSIGNED_BYTE && (glFormat == glFormat_GL_RGB || glFormat == glFormat_GL_RGB8))
    {
        LOGE("VK_FORMAT_R8G8B8_UNORM / VK_FORMAT_R8G8B8_SRGB Texture format NOT tested!");
        if (glInternalFormat == glFormat_GL_SRGB8_EXT)
        {
            RetVal = VK_FORMAT_R8G8B8_SRGB;
        }
        else
        {
            RetVal = VK_FORMAT_R8G8B8_UNORM;
        }
    }

    else if (glType == glType_GL_UNSIGNED_BYTE && (glFormat == glFormat_GL_RGBA || glFormat == glFormat_GL_RGBA8))
    {
        if (glInternalFormat == glFormat_GL_SRGB8_ALPHA8_EXT)
        {
            RetVal = VK_FORMAT_R8G8B8A8_SRGB;
        }
        else
        {
            RetVal = VK_FORMAT_R8G8B8A8_UNORM;
        }
    }

    // Float Formats
    else if (glType == glType_GL_HALF_FLOAT && glInternalFormat == glFormat_GL_RGBA16F)
    {
        RetVal = VK_FORMAT_R16G16B16A16_SFLOAT;
    }

    else if (glType == glType_GL_FLOAT && glInternalFormat == glFormat_GL_RGBA32F)
    {
        LOGE("VK_FORMAT_R32G32B32A32_SFLOAT Texture format NOT tested!");
        RetVal = VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    // Compressed Formats
    else
    {
        const auto it = CompressedGlFormatToVkFormat.find( glInternalFormat );
        if( it != CompressedGlFormatToVkFormat.end() )
        {
            RetVal = it->second;
        }
    }

    // Make sure we found something
    if (RetVal == VK_FORMAT_UNDEFINED)
    {
        LOGE("KTX texture formats could not be converted to Vulkan! Type = 0x%x; Format = 0x%x", glType, glFormat);
    }

    return RetVal;
}

//-----------------------------------------------------------------------------
bool L_ParseKTXBuffer(const char* pFileName, void* pKTXBuffer, uint32_t BufferLength, VulkanTexData* pTexData)
//-----------------------------------------------------------------------------
{
    if (pTexData == NULL)
        return false;

    // Make sure starting from a clean place
    L_FreeTexData(pTexData);

    // Set up the walker
    void* pWalker = pKTXBuffer;
    uint32_t uiWalkerDist = 0;

    // Read and verify the KTX Header
    KTXHeader* pHeader = (KTXHeader*)pWalker;

    if (memcmp(pHeader->identifier, KTX_IDENTIFIER_REF.data(), KTX_IDENTIFIER_REF.size()) != 0)
    {
        LOGE("KTX file has invalid header: %s", pFileName);
        return false;
    }

    if( pHeader->endianness != KTX_ENDIAN_REF )
    {
        LOGE("KTX file has invalid endianness (loader does not currently support little endian): %s", pFileName);
        return false;
    }

    // Skip over the header
    uiWalkerDist += sizeof(KTXHeader);
    pWalker = (char*)pKTXBuffer + uiWalkerDist;

    // Skip over key value data
    uiWalkerDist += pHeader->bytesOfKeyValueData;
    pWalker = (char*)pKTXBuffer + uiWalkerDist;

    // LOGI("Texture Header Info (%s)", pFileName);
    // LOGI("    glType: 0x%x", pHeader->glType);
    // LOGI("    glTypeSize: %d", pHeader->glTypeSize);
    // LOGI("    glFormat: 0x%x", pHeader->glFormat);
    // LOGI("    glInternalFormat: 0x%x", pHeader->glInternalFormat);
    // LOGI("    glBaseInternalFormat: 0x%x", pHeader->glBaseInternalFormat);
    // LOGI("    pixelWidth: %d", pHeader->pixelWidth);
    // LOGI("    pixelHeight: %d", pHeader->pixelHeight);
    // LOGI("    pixelDepth: %d", pHeader->pixelDepth);
    // LOGI("    numberOfArrayElements: %d", pHeader->numberOfArrayElements);
    // LOGI("    numberOfFaces: %d", pHeader->numberOfFaces);
    // LOGI("    numberOfMipmapLevels: %d", pHeader->numberOfMipmapLevels);

    // ********************************
    // Memory Allocation
    // ********************************
    // Each texture has faces...
    pTexData->pFaceData = (FaceTexData*)malloc(pHeader->numberOfFaces * sizeof(FaceTexData));
    if (pTexData->pFaceData == NULL)
    {
        LOGE("Unable to allocate memory for %d faces: %s", pHeader->numberOfFaces, pFileName);
        return false;
    }

    // Pull off the info we care about
    pTexData->glType = pHeader->glType;
    pTexData->glFormat = pHeader->glFormat;
    pTexData->glInternalFormat = pHeader->glInternalFormat;
    pTexData->VulkanFormat = L_ReturnVulkanFormat(pHeader->glType, pHeader->glFormat, pHeader->glInternalFormat);
    pTexData->NumFaces = pHeader->numberOfFaces;

    for (uint32_t WhichFace = 0; WhichFace < pTexData->NumFaces; WhichFace++)
    {
        // ...each face has layers...

        // For loop, we need at least one
        if (pHeader->numberOfArrayElements == 0)
            pHeader->numberOfArrayElements = 1;

        pTexData->pFaceData[WhichFace].pLayerData = (LayerTexData*)malloc(pHeader->numberOfArrayElements * sizeof(LayerTexData));
        if (pTexData->pFaceData[WhichFace].pLayerData == NULL)
        {
            LOGE("Unable to allocate memory for %d layers: %s", pHeader->numberOfArrayElements, pFileName);
            return false;
        }
        pTexData->pFaceData[WhichFace].NumLayers = pHeader->numberOfArrayElements;

        for (uint32_t WhichLayer = 0; WhichLayer < pTexData->pFaceData[WhichFace].NumLayers; WhichLayer++)
        {
            // ...each layer has mip levels

            // For loop, we need at least one
            if (pHeader->numberOfMipmapLevels == 0)
                pHeader->numberOfMipmapLevels = 1;

            pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData = (MipTexData*)malloc(pHeader->numberOfMipmapLevels * sizeof(MipTexData));
            if (pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData == NULL)
            {
                LOGE("Unable to allocate memory for %d mip levels: %s", pHeader->numberOfMipmapLevels, pFileName);
                return false;
            }
            pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].NumMipLevels = pHeader->numberOfMipmapLevels;

            for (uint32_t WhichMipLevel = 0; WhichMipLevel < pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].NumMipLevels; WhichMipLevel++)
            {
                // These will be allocated and filled in later.  Set to NULL for error checking
                pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMipLevel].pData = NULL;
            }   // Which MipLevel

        }   // Which Layer

    }   // Which Face

    // ********************************
    // Allocate and fill mip levels
    // ********************************
    uint32_t uiMipWidth = pHeader->pixelWidth;
    uint32_t uiMipHeight = pHeader->pixelHeight;
    uint32_t uiMipSize = 0;
    for (uint32_t WhichMipLevel = 0; WhichMipLevel < pHeader->numberOfMipmapLevels; WhichMipLevel++)
    {
        // What is the data size of this mip level
        uiMipSize = *((uint32_t*)pWalker);
        uiWalkerDist += sizeof(uint32_t);
        pWalker = (char*)pKTXBuffer + uiWalkerDist;

        // LOGI("Mip %d: %dx%d => %d bytes", WhichMipLevel, uiMipWidth, uiMipHeight, uiMipSize);

        for (uint32_t WhichLayer = 0; WhichLayer < pHeader->numberOfArrayElements; WhichLayer++)
        {
            for (uint32_t WhichFace = 0; WhichFace < pHeader->numberOfFaces; WhichFace++)
            {
                // TODO: for (uint32_t WhichSlice = 0; WhichSlice < pHeader->pixelDepth; WhichSlice++)

                // Allocate this mip level...
                void* pTempData = malloc(uiMipSize);
                if (pTempData == NULL)
                {
                    LOGE("Unable to allocate %d bytes of memory for mip level: %s", uiMipSize, pFileName);
                    return false;
                }

                // ...copy the data over...
                memcpy(pTempData, pWalker, uiMipSize);

                // ... and set the data in the structure
                pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMipLevel].Width = uiMipWidth;
                pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMipLevel].Height = uiMipHeight;
                pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMipLevel].Size = uiMipSize;
                pTexData->pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMipLevel].pData = pTempData;


                // Step forward but make sure on the correct boundary
                uiWalkerDist += uiMipSize;
                pWalker = (char*)pKTXBuffer + uiWalkerDist;

                // Possible to have face padding here
                // (https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/) 
                // cubePadding: "For non-array cubemap textures (any texture where numberOfFaces is 6 and 
                // numberOfArrayElements is 0) cubePadding contains between 0 and 3 bytes to ensure that the 
                // data in each face begins at a file offset that is a multiple of 4. In all other cases 
                // cubePadding is empty (0 bytes long)."
                uint32_t CubePadding = (uiWalkerDist % 4);
                if (CubePadding != 0)
                {
                    uiWalkerDist += CubePadding;
                    pWalker = (char*)pKTXBuffer + uiWalkerDist;
                }

            }   // Which Face
        }   // Which Layer

        // Divide the mip size to go to next one
        uiMipWidth /= 2;
        uiMipHeight /= 2;

        // Make sure we don't have a size of zero
        if (uiMipWidth == 0)
            uiMipWidth = 1;
        if (uiMipHeight == 0)
            uiMipHeight = 1;

        // May need to adjust pointer due to padding (https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/)
        uint32_t BytePadding = 3 - ((uiMipSize + 3) % 4);
        if (BytePadding != 0)
        {
            uiWalkerDist += BytePadding;
            pWalker = (char*)pKTXBuffer + uiWalkerDist;
        }
    }   // Which MipLevel

    // Everything worked out
    return true;
}


//-----------------------------------------------------------------------------
VulkanTexInfo LoadKTXTexture(Vulkan* pVulkan, AssetManager& assetManager, const char* pFileName, VkSamplerAddressMode SamplerMode)
//-----------------------------------------------------------------------------
{
    // Texture Convert Command Line: simpletextureconverter hud.tga hud.ktx -format R8G8B8A8Unorm -flipY

    VkResult RetVal;

    uint32_t uiWidth = 0;
    uint32_t uiHeight = 0;
    uint32_t uiFaces = 0;
    uint32_t uiMipLevels = 0;
    VkFormat VulkanFormat = VK_FORMAT_UNDEFINED;
    bool forceLinearTiling = false;

    VkFormatProperties formatProperties = {};

    VulkanTexData TexData = {};

    LOGI("Loading KTX texture: %s", pFileName);

    {
        std::vector<char> fileData;
        if (!assetManager.LoadFileIntoMemory(pFileName, fileData))
        {
            LOGE("Error reading texture file: %s", pFileName);
            return {};
        }

        if (!L_ParseKTXBuffer(pFileName, fileData.data(), (uint32_t)fileData.size(), &TexData))
        {
            LOGE("Error parsing texture file: %s", pFileName);
            return {};
        }
    }

    uiWidth = TexData.pFaceData[0].pLayerData[0].pMipData[0].Width;
    uiHeight = TexData.pFaceData[0].pLayerData[0].pMipData[0].Height;
    uiFaces = TexData.NumFaces;
    uiMipLevels = TexData.pFaceData[0].pLayerData[0].NumMipLevels;
    VulkanFormat = TexData.VulkanFormat;

    // Check that the device supports this format
    if( !pVulkan->IsTextureFormatSupported( VulkanFormat ))
    {
        LOGE("Error, texture format not supported by device: %s", pFileName);
        return {};
    }

	// Get device properites for the requested texture format
	vkGetPhysicalDeviceFormatProperties(pVulkan->m_VulkanGpu, VulkanFormat, &formatProperties);
	bool useStaging = !(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

	// Image creation info.  Will change below based on need
    VkImageCreateInfo ImageInfo {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ImageInfo.flags = 0;
	ImageInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageInfo.format = VulkanFormat;
	ImageInfo.extent.width = uiWidth;
	ImageInfo.extent.height = uiHeight;
    ImageInfo.extent.depth = 1; // Spec says for VK_IMAGE_TYPE_2D depth must be 1
	ImageInfo.mipLevels = 1;	// Set to 1 since making each level.  Later will be set to uiMipLevels;
	ImageInfo.arrayLayers = 1;  // TODO: This must change to support 3D textures or arrays?  Set to 6 for cube maps
	ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	ImageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	ImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageInfo.queueFamilyIndexCount = 0;
	ImageInfo.pQueueFamilyIndices = NULL;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;   // VK_IMAGE_LAYOUT_UNDEFINED says data not guaranteed to be preserved when changing state

    VkMemoryAllocateInfo AllocInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	AllocInfo.allocationSize = 0;	// Will be filled in below
	AllocInfo.memoryTypeIndex = 0;

    VkMemoryRequirements MemReqs = {};

	struct CubeFace
	{
        CubeFace(uint32_t uiFaces) : mipImages(uiFaces) {}
        std::vector<Wrap_VkImage> mipImages;
	};

    // Allocate the cube face and mips for each face
    std::vector<CubeFace> cubeFaces;
    cubeFaces.reserve(uiFaces);

    for (uint32_t WhichFace = 0; WhichFace < uiFaces; WhichFace++)
    {
        cubeFaces.emplace_back(uiMipLevels);
    }

	// Create and copy mip levels

    // Need the setup command buffer for loading images
    VkCommandBuffer SetupCmdBuffer = pVulkan->StartSetupCommandBuffer();

	// Load separate cube map faces into linear tiled textures
    for (uint32_t WhichFace = 0; WhichFace < uiFaces; WhichFace++)
    {
        for (uint32_t WhichMip = 0; WhichMip < uiMipLevels; WhichMip++)
        {
            // TODO: Layers are not supported
            uint32_t WhichLayer = 0;

            ImageInfo.extent.width = TexData.pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMip].Width;
            ImageInfo.extent.height = TexData.pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMip].Height;

            char szName[256];
            memset(szName, 0, sizeof(szName));
            sprintf(szName, "%s: Face %d; Mip %d", pFileName, WhichFace, WhichMip);
            if (!cubeFaces[WhichFace].mipImages[WhichMip].Initialize(pVulkan, ImageInfo, MemoryManager::MemoryUsage::CpuToGpu))
            {
                LOGE("Unable to initialize mip %d of face %d (%s)", WhichMip + 1, WhichFace + 1, pFileName);
                return {};
            }

            // ... copy texture data into the image
            VkImageSubresource SubresInfo {};
            SubresInfo.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            SubresInfo.mipLevel = 0;    // This is always 0 since dealing with temporary images
            SubresInfo.arrayLayer = 0;  // TODO: Adjust this as we support 3D/array textures

            VkSubresourceLayout SubResLayout {};
            auto& faceImage = cubeFaces[WhichFace].mipImages[WhichMip].m_VmaImage;
            vkGetImageSubresourceLayout(pVulkan->m_VulkanDevice, faceImage.GetVkBuffer(), &SubresInfo, &SubResLayout);

            {
                // account for SubResLayout.offset too?
                MemoryManager& memorymanager = pVulkan->GetMemoryManager();
                auto mappedMemory = memorymanager.Map<void>(faceImage);
                uint8_t* pDst = (uint8_t*)mappedMemory.data();
                uint8_t* pSrc = (uint8_t*)TexData.pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMip].pData;
                if (Vulkan::FormatIsCompressed(VulkanFormat) || SubResLayout.rowPitch * ImageInfo.extent.height == TexData.pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMip].Size)
                {
                    // Block sizes match, copy entire mip level.
                    memcpy(pDst, pSrc, TexData.pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMip].Size);
                }
                else
                {
                    // Pitch is such that we need to copy line by line - can happen on non power-2 textures and when texture size gets very small (depending on GPU memory alignment requirements)
                    size_t srcRowPitch = TexData.pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMip].Size / ImageInfo.extent.height;
                    for (uint32_t y = 0; y < ImageInfo.extent.height; ++y)
                    {
                        memcpy(pDst, pSrc, srcRowPitch);
                        pDst += SubResLayout.rowPitch;
                        pSrc += srcRowPitch;
                    }
                }
                memorymanager.Unmap(faceImage, std::move(mappedMemory));
            }

            // Image barrier for linear image (base)
            // Linear image will be used as a source for the copy
            VkPipelineStageFlags srcMask = 0;
            VkPipelineStageFlags dstMask = 1;
            uint32_t baseMipLevel = 0;
            uint32_t mipLevelCount = 1;

            pVulkan->SetImageLayout(cubeFaces[WhichFace].mipImages[WhichMip].m_VmaImage.GetVkBuffer(), 
                SetupCmdBuffer, 
                VK_IMAGE_ASPECT_COLOR_BIT, 
                VK_IMAGE_LAYOUT_PREINITIALIZED, 
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                srcMask,
                dstMask,
                baseMipLevel,
                mipLevelCount);
        }   // Each mipmap in each face
    }   // Each Face

    // Transfer cube map faces to optimal tiling

    // Now that creating the whole thing we need the correct size
    ImageInfo.extent.width = uiWidth;
    ImageInfo.extent.height = uiHeight;

    // Now that we are done creating single images, need to create all mip levels
    ImageInfo.mipLevels = uiMipLevels;

	// Setup texture as blit target with optimal tiling
	ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (uiFaces == 6)
        ImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    ImageInfo.arrayLayers = uiFaces;

    // Need the return image
    Wrap_VkImage RetImage;
    if(!RetImage.Initialize(pVulkan, ImageInfo, MemoryManager::MemoryUsage::GpuExclusive, pFileName))
    {
        LOGE("Unable to initialize texture image (%s)", pFileName);
        return {};
    }

    // Image barrier for optimal image (target)
	// Optimal image will be used as destination for the copy
    VkPipelineStageFlags srcMask = 0;
    VkPipelineStageFlags dstMask = 1;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = uiMipLevels;
    uint32_t baseLayer = 0;
    uint32_t layerCount = uiFaces;

    pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(),
        SetupCmdBuffer,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_PREINITIALIZED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        srcMask,
        dstMask,
        baseMipLevel,
        mipLevelCount,
        baseLayer,
        layerCount);

    // Copy cube map faces one by one
    // Vulkan spec says the order is +X, -X, +Y, -Y, +Z, -Z
    for (uint32_t WhichFace = 0; WhichFace < uiFaces; WhichFace++)
    {
        for (uint32_t WhichMip = 0; WhichMip < uiMipLevels; WhichMip++)
        {
            // TODO: Layers are not supported
            uint32_t WhichLayer = 0;

            // Copy region for image blit
		    VkImageCopy copyRegion = {};

            // Source is always base level because we have an image for each face and each mip
		    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		    copyRegion.srcSubresource.baseArrayLayer = 0;
		    copyRegion.srcSubresource.mipLevel = 0;
		    copyRegion.srcSubresource.layerCount = 1;
		    copyRegion.srcOffset.x = 0;
            copyRegion.srcOffset.y = 0;
            copyRegion.srcOffset.z = 0;

            // Source is the section of the main image
		    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		    copyRegion.dstSubresource.baseArrayLayer = WhichFace;
		    copyRegion.dstSubresource.mipLevel = WhichMip;
		    copyRegion.dstSubresource.layerCount = 1;
		    copyRegion.dstOffset.x = 0;
            copyRegion.dstOffset.y = 0;
            copyRegion.dstOffset.z = 0;

            // Size is the size of this mip
            copyRegion.extent.width = TexData.pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMip].Width;
            copyRegion.extent.height = TexData.pFaceData[WhichFace].pLayerData[WhichLayer].pMipData[WhichMip].Height;

		    copyRegion.extent.depth = 1;

            // Put image copy into command buffer
		     vkCmdCopyImage(
                 SetupCmdBuffer,
                 cubeFaces[WhichFace].mipImages[WhichMip].m_VmaImage.GetVkBuffer(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 RetImage.m_VmaImage.GetVkBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			     1, &copyRegion);
        }   // Each mipmap in each face
    }   // Each Face

    // No longer need the texture data
    L_FreeTexData(&TexData);

    // Change texture image layout to shader read after the copy
    VkImageLayout RetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    srcMask = 0;
    dstMask = 1;
    baseMipLevel = 0;
    mipLevelCount = uiMipLevels;
    baseLayer = 0;
    layerCount = uiFaces;

    pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, RetImageLayout,
        srcMask,
        dstMask,
        baseMipLevel,
        mipLevelCount,
        baseLayer,
        layerCount);

    // Submit the command buffer we have been working on
    pVulkan->FinishSetupCommandBuffer(SetupCmdBuffer);

	// Need a sampler...
	VkSamplerCreateInfo SamplerInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	SamplerInfo.flags = 0;
	SamplerInfo.magFilter = VK_FILTER_LINEAR;
	SamplerInfo.minFilter = VK_FILTER_LINEAR;
	SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;             // TODO: We don't currently support mipmaps
	SamplerInfo.addressModeU = SamplerMode;
	SamplerInfo.addressModeV = SamplerMode;
	SamplerInfo.addressModeW = SamplerMode;
	SamplerInfo.mipLodBias = 0.0f;
	SamplerInfo.anisotropyEnable = VK_FALSE;
	SamplerInfo.maxAnisotropy = 8.0f;
	SamplerInfo.compareEnable = VK_FALSE;
    SamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	SamplerInfo.minLod = 0.0f;
    SamplerInfo.maxLod = FLT_MAX;   //  (float)uiMipLevels;
	SamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;   // VkBorderColor
	SamplerInfo.unnormalizedCoordinates = VK_FALSE;

	VkSampler RetSampler;
	RetVal = vkCreateSampler(pVulkan->m_VulkanDevice, &SamplerInfo, NULL, &RetSampler);
	if (!CheckVkError("vkCreateSampler()", RetVal))
	{
        return {};
	}

	// ... and an ImageView
	VkImageViewCreateInfo ImageViewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	ImageViewInfo.flags = 0;
	ImageViewInfo.image = RetImage.m_VmaImage.GetVkBuffer();
    if (uiFaces == 6)
        ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else
        ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

	ImageViewInfo.format = ImageInfo.format;
	ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ImageViewInfo.subresourceRange.baseMipLevel = 0;
    ImageViewInfo.subresourceRange.levelCount = uiMipLevels;
	ImageViewInfo.subresourceRange.baseArrayLayer = 0;
	ImageViewInfo.subresourceRange.layerCount = 1;
    if (uiFaces == 6)
        ImageViewInfo.subresourceRange.layerCount = 6;
    else
        ImageViewInfo.subresourceRange.layerCount = 1;

	VkImageView RetImageView;
	RetVal = vkCreateImageView(pVulkan->m_VulkanDevice, &ImageViewInfo, NULL, &RetImageView);
	if (!CheckVkError("vkCreateImageView()", RetVal))
	{
        return {};
	}

    // LOGI("vkCreateImageView: %s -> %p", pFileName, RetImageView);


	// Cleanup
    cubeFaces.clear();


	// ****************************************************
	// ****************************************************

    // Set the return values
    VulkanTexInfo RetTex{ uiWidth, uiHeight, uiMipLevels, ImageInfo.format, RetImageLayout, std::move(RetImage.m_VmaImage), RetSampler, RetImageView };
    return RetTex;

}

//-----------------------------------------------------------------------------
VulkanTexInfo LoadTextureFromBuffer(Vulkan* pVulkan, void* pData, uint32_t DataSize, uint32_t Width, uint32_t Height, VkFormat NewFormat, VkSamplerAddressMode SamplerMode, VkFilter Filter, VkImageUsageFlags FinalUsage, VkImageLayout FinalLayout)
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    uint32_t uiWidth = Width;
    uint32_t uiHeight = Height;
    uint32_t uiFaces = 1;
    uint32_t uiMipLevels = 1;
    VkFormat VulkanFormat = NewFormat;

    // Image creation info.  Will change below based on need
    VkImageCreateInfo ImageInfo {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ImageInfo.flags = 0;
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.format = VulkanFormat;
    ImageInfo.extent.width = uiWidth;
    ImageInfo.extent.height = uiHeight;
    ImageInfo.extent.depth = 1; // Spec says for VK_IMAGE_TYPE_2D depth must be 1
    ImageInfo.mipLevels = 1;	// Set to 1 since making each level.  Later will be set to uiMipLevels;
    ImageInfo.arrayLayers = 1;  // TODO: This must change to support 3D textures or arrays?  Set to 6 for cube maps
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_LINEAR;
    ImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.queueFamilyIndexCount = 0;
    ImageInfo.pQueueFamilyIndices = NULL;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;   // VK_IMAGE_LAYOUT_UNDEFINED says data not guaranteed to be preserved when changing state

    VkMemoryAllocateInfo AllocInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkMemoryRequirements MemReqs {};

    struct CubeFace
    {
        CubeFace(uint32_t uiFaces) : mipImages(uiFaces) {}
        std::vector<Wrap_VkImage> mipImages;
    };

    // Allocate the cube face and mips for each face
    std::vector<CubeFace> cubeFaces;
    cubeFaces.reserve(uiFaces);

    for (uint32_t WhichFace = 0; WhichFace < uiFaces; WhichFace++)
    {
        cubeFaces.emplace_back(uiMipLevels);
    }

    // Create and copy mip levels

    // Need the setup command buffer for loading images
    VkCommandBuffer SetupCmdBuffer = pVulkan->StartSetupCommandBuffer();

    // Load separate cube map faces into linear tiled textures
    for (uint32_t WhichFace = 0; WhichFace < uiFaces; WhichFace++)
    {
        for (uint32_t WhichMip = 0; WhichMip < uiMipLevels; WhichMip++)
        {
            // TODO: Layers are not supported
            uint32_t WhichLayer = 0;

            ImageInfo.extent.width = Width;
            ImageInfo.extent.height = Height;

            if (!cubeFaces[WhichFace].mipImages[WhichMip].Initialize(pVulkan, ImageInfo, MemoryManager::MemoryUsage::CpuToGpu))
            {
                LOGE("LoadTextureFromBuffer: Unable to initialize mip %d of face %d", WhichMip + 1, WhichFace + 1);
                return {};
            }

            // ... copy texture data into the image
            VkImageSubresource SubresInfo = {};
            memset(&SubresInfo, 0, sizeof(VkImageSubresource));
            SubresInfo.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            SubresInfo.mipLevel = 0;    // This is always 0 since dealing with temporary images
            SubresInfo.arrayLayer = 0;  // TODO: Adjust this as we support 3D/array textures

            VkSubresourceLayout SubResLayout;
            memset(&SubResLayout, 0, sizeof(VkSubresourceLayout));
            auto& faceImage = cubeFaces[WhichFace].mipImages[WhichMip].m_VmaImage;
            vkGetImageSubresourceLayout(pVulkan->m_VulkanDevice, faceImage.GetVkBuffer(), &SubresInfo, &SubResLayout);

            {
                MemoryManager& memorymanager = pVulkan->GetMemoryManager();
                auto mappedMemory = memorymanager.Map<void>(faceImage);
                memcpy(mappedMemory.data(), pData, DataSize);
                memorymanager.Unmap(faceImage, std::move(mappedMemory));
            }

            // Image barrier for linear image (base)
            // Linear image will be used as a source for the copy
            VkPipelineStageFlags srcMask = 0;
            VkPipelineStageFlags dstMask = 1;
            uint32_t baseMipLevel = 0;
            uint32_t mipLevelCount = 1;

            pVulkan->SetImageLayout(cubeFaces[WhichFace].mipImages[WhichMip].m_VmaImage.GetVkBuffer(),
                SetupCmdBuffer,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_PREINITIALIZED,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                srcMask,
                dstMask,
                baseMipLevel,
                mipLevelCount);
        }   // Each mipmap in each face
    }   // Each Face

    // Transfer cube map faces to optimal tiling

    // Now that creating the whole thing we need the correct size
    ImageInfo.extent.width = uiWidth;
    ImageInfo.extent.height = uiHeight;

    // Now that we are done creating single images, need to create all mip levels
    ImageInfo.mipLevels = uiMipLevels;

    // Setup texture as blit target with optimal tiling
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.usage = FinalUsage;

    if (uiFaces == 6)
        ImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    ImageInfo.arrayLayers = uiFaces;

    // Need the return image
    Wrap_VkImage RetImage;
    if (!RetImage.Initialize(pVulkan, ImageInfo, MemoryManager::MemoryUsage::GpuExclusive))
    {
        LOGE("LoadTextureFromBuffer: Unable to initialize texture image");
        return {};
    }

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    VkPipelineStageFlags srcMask = 0;
    VkPipelineStageFlags dstMask = 1;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = uiMipLevels;
    uint32_t baseLayer = 0;
    uint32_t layerCount = uiFaces;

    pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(),
        SetupCmdBuffer,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_PREINITIALIZED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        srcMask,
        dstMask,
        baseMipLevel,
        mipLevelCount,
        baseLayer,
        layerCount);

    // Copy cube map faces one by one
    // Vulkan spec says the order is +X, -X, +Y, -Y, +Z, -Z
    for (uint32_t WhichFace = 0; WhichFace < uiFaces; WhichFace++)
    {
        for (uint32_t WhichMip = 0; WhichMip < uiMipLevels; WhichMip++)
        {
            // TODO: Layers are not supported
            uint32_t WhichLayer = 0;

            // Copy region for image blit
            VkImageCopy copyRegion = {};
            memset(&copyRegion, 0, sizeof(VkImageCopy));

            // Source is always base level because we have an image for each face and each mip
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset.x = 0;
            copyRegion.srcOffset.y = 0;
            copyRegion.srcOffset.z = 0;

            // Source is the section of the main image
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = WhichFace;
            copyRegion.dstSubresource.mipLevel = WhichMip;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset.x = 0;
            copyRegion.dstOffset.y = 0;
            copyRegion.dstOffset.z = 0;

            // Size is the size of this mip
            copyRegion.extent.width = Width;
            copyRegion.extent.height = Height;

            copyRegion.extent.depth = 1;

            // Put image copy into command buffer
            vkCmdCopyImage(
                SetupCmdBuffer,
                cubeFaces[WhichFace].mipImages[WhichMip].m_VmaImage.GetVkBuffer(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                RetImage.m_VmaImage.GetVkBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &copyRegion);
        }   // Each mipmap in each face
    }   // Each Face

    // Change texture image layout to the 'final' settings now we are done transferring
    srcMask = 0;
    dstMask = 1;
    baseMipLevel = 0;
    mipLevelCount = uiMipLevels;
    baseLayer = 0;
    layerCount = uiFaces;

    pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, FinalLayout,
        srcMask,
        dstMask,
        baseMipLevel,
        mipLevelCount,
        baseLayer,
        layerCount);

    // Submit the command buffer we have been working on
    pVulkan->FinishSetupCommandBuffer(SetupCmdBuffer);

    // Need a sampler...
    VkSamplerCreateInfo SamplerInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    SamplerInfo.flags = 0;
    SamplerInfo.magFilter = Filter; // VK_FILTER_LINEAR;
    SamplerInfo.minFilter = Filter; // VK_FILTER_LINEAR;
    SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;             // TODO: We don't currently support mipmaps
    SamplerInfo.addressModeU = SamplerMode;
    SamplerInfo.addressModeV = SamplerMode;
    SamplerInfo.addressModeW = SamplerMode;
    SamplerInfo.mipLodBias = 0.0f;
    SamplerInfo.anisotropyEnable = VK_FALSE;
    SamplerInfo.maxAnisotropy = 8.0f;
    SamplerInfo.compareEnable = VK_FALSE;
    SamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    SamplerInfo.minLod = 0.0f;
    SamplerInfo.maxLod = FLT_MAX;   //  (float)uiMipLevels;
    SamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;   // VkBorderColor
    SamplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler RetSampler;
    RetVal = vkCreateSampler(pVulkan->m_VulkanDevice, &SamplerInfo, NULL, &RetSampler);
    if (!CheckVkError("vkCreateSampler()", RetVal))
    {
        return {};
    }

    // ... and an ImageView
    VkImageViewCreateInfo ImageViewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ImageViewInfo.flags = 0;
    ImageViewInfo.image = RetImage.m_VmaImage.GetVkBuffer();
    if (uiFaces == 6)
        ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else
        ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    ImageViewInfo.format = ImageInfo.format;
    ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageViewInfo.subresourceRange.baseMipLevel = 0;
    ImageViewInfo.subresourceRange.levelCount = uiMipLevels;
    ImageViewInfo.subresourceRange.baseArrayLayer = 0;
    ImageViewInfo.subresourceRange.layerCount = 1;
    if (uiFaces == 6)
        ImageViewInfo.subresourceRange.layerCount = 6;
    else
        ImageViewInfo.subresourceRange.layerCount = 1;

    VkImageView RetImageView;
    RetVal = vkCreateImageView(pVulkan->m_VulkanDevice, &ImageViewInfo, NULL, &RetImageView);
    if (!CheckVkError("vkCreateImageView()", RetVal))
    {
        return {};
    }

    // Cleanup
    cubeFaces.clear();


    // ****************************************************
    // ****************************************************

    // Set the return values
    VulkanTexInfo RetTex{ uiWidth, uiHeight, uiMipLevels, ImageInfo.format, FinalLayout, std::move( RetImage.m_VmaImage ), RetSampler, RetImageView };
    return RetTex;
}


//-----------------------------------------------------------------------------
void ReleaseTexture(Vulkan* pVulkan, VulkanTexInfo* pTexInfo)
//-----------------------------------------------------------------------------
{
    pTexInfo->Release(pVulkan);

    *pTexInfo = VulkanTexInfo{};    // destroy and clear
}

//
// Constructors/move-operators for VulkanTexInfo.
// Ensures we are not leaking Samplers, ImageViews or memory.
//
VulkanTexInfo::VulkanTexInfo(VulkanTexInfo&& other) noexcept
{
    *this = std::move(other);
}
VulkanTexInfo& VulkanTexInfo::operator=(VulkanTexInfo&& other) noexcept
{
    if (this != &other)
    {
        // Some of the data can be copied and is ok to leave 'other' alone (move operator just has to ensure the 'other' is in a valid state and can be safely deleted)
        Width = other.Width;
        Height = other.Height;
        MipLevels = other.MipLevels;
        Format = other.Format;
        ImageLayout = other.ImageLayout;
        // Actually transfer ownership from 'other'
        VmaImage = std::move(other.VmaImage);
        Sampler = other.Sampler;
        other.Sampler = VK_NULL_HANDLE;
        ImageView = other.ImageView;
        other.ImageView = VK_NULL_HANDLE;
        Image = other.Image;
        other.Image = VK_NULL_HANDLE;
        Memory = other.Memory;
        other.Memory = VK_NULL_HANDLE;
    }
    return *this;
}
VulkanTexInfo::~VulkanTexInfo()
{
    // Asserts to ensure we called ReleaseTexture on this already.
    assert(!VmaImage);
    assert(Sampler == VK_NULL_HANDLE);
    assert(ImageView == VK_NULL_HANDLE);
}


void VulkanTexInfo::Release( Vulkan* pVulkan )
{
    if( ImageView != VK_NULL_HANDLE )
        vkDestroyImageView( pVulkan->m_VulkanDevice, ImageView, NULL );
    ImageView = VK_NULL_HANDLE;

    if( Sampler != VK_NULL_HANDLE )
        vkDestroySampler( pVulkan->m_VulkanDevice, Sampler, NULL );
    Sampler = VK_NULL_HANDLE;

    if( VmaImage )
        pVulkan->GetMemoryManager().Destroy( std::move( VmaImage ) );

    Image = VK_NULL_HANDLE;
    Memory = VK_NULL_HANDLE;
}

//-----------------------------------------------------------------------------
VulkanTexInfo CreateTextureObject(Vulkan* pVulkan, uint32_t uiWidth, uint32_t uiHeight, VkFormat NewFormat, TEXTURE_TYPE TexType, const char* pName, VkSampleCountFlagBits Msaa)
//-----------------------------------------------------------------------------
{
    VkResult RetVal;

    // ******************************************
    // This Routine same as TextureFuncts.cpp, LoadKTXTexture()
    // TODO: Combine them!
    // ******************************************
    VkSampler RetSampler = VK_NULL_HANDLE;
    VkImageView RetImageView = VK_NULL_HANDLE;
    VkImageLayout RetImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // How this texture object will be used.
    MemoryManager::MemoryUsage MemoryUsage = MemoryManager::MemoryUsage::GpuExclusive;

    // Create the image...
    VkImageCreateInfo ImageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ImageInfo.flags = 0;
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.format = NewFormat;
    ImageInfo.extent.width = uiWidth;
    ImageInfo.extent.height = uiHeight;
    ImageInfo.extent.depth = 1; // Spec says for VK_IMAGE_TYPE_2D depth must be 1
    ImageInfo.mipLevels = 1;    // TODO: This must change to support mipmaps
    ImageInfo.arrayLayers = 1;  // TODO: This must change to support 3D textures or arrays (Cubemaps)
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; // VK_IMAGE_TILING_LINEAR
    ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.queueFamilyIndexCount = 0;
    ImageInfo.pQueueFamilyIndices = NULL;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;   //  VK_IMAGE_LAYOUT_UNDEFINED;    // Don't need VK_IMAGE_LAYOUT_PREINITIALIZED since not initializing?

    switch (TexType)
    {
    case TT_NORMAL:
        ImageInfo.tiling = VK_IMAGE_TILING_LINEAR;      // VK_IMAGE_TILING_OPTIMAL
        ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        break;
    case TT_RENDER_TARGET:
        // If using VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT then tiling MUST be VK_IMAGE_TILING_OPTIMAL
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        if (Msaa == VK_SAMPLE_COUNT_1_BIT)
            ImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        else
            ImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ImageInfo.samples = Msaa;
        break;
    case TT_RENDER_TARGET_SUBPASS:
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;     // VK_IMAGE_TILING_LINEAR will NOT work
        if (Msaa == VK_SAMPLE_COUNT_1_BIT)
            ImageInfo.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        else
            ImageInfo.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        ImageInfo.samples = Msaa;
        // Subpass render targets dont need to be backed by memory!
        MemoryUsage = MemoryManager::MemoryUsage::GpuLazilyAllocated;
        break;
    case TT_COMPUTE_TARGET:
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;     // VK_IMAGE_TILING_LINEAR will NOT work
        ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    case TT_DEPTH_TARGET:
        // If using VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT then tiling MUST be VK_IMAGE_TILING_OPTIMAL
        ImageInfo.mipLevels = 1;
        ImageInfo.arrayLayers = 1;
        ImageInfo.samples = Msaa;
        ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if( Msaa != VK_SAMPLE_COUNT_1_BIT )
            ImageInfo.flags |= VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT;
        break;

    default:
        assert(0);
        break;
    }

    // Need the return image
    Wrap_VkImage RetImage;
    bool ImageInitialized = RetImage.Initialize( pVulkan, ImageInfo, MemoryUsage, pName );
    if( !ImageInitialized && MemoryUsage == MemoryManager::MemoryUsage::GpuLazilyAllocated )
    {
        LOGI( "Unable to initialize GpuLazilyAllocated image (probably not supported by GPU hardware).  Falling back to GpuExclusive" );
        MemoryUsage = MemoryManager::MemoryUsage::GpuExclusive;
        ImageInfo.usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        ImageInitialized = RetImage.Initialize( pVulkan, ImageInfo, MemoryUsage, pName );
    }
    if (!ImageInitialized)
    {
        LOGE("Unable to initialize image (Not from file)");
        return {};
    }

    VkCommandBuffer SetupCmdBuffer = pVulkan->StartSetupCommandBuffer();
    switch (TexType)
    {
    case TT_NORMAL:
        pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 1);
        RetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case TT_RENDER_TARGET:
        pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 0, 1);
        RetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case TT_RENDER_TARGET_SUBPASS:
        RetImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        break;
    case TT_COMPUTE_TARGET:
        pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_GENERAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 0, 1);
        RetImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        break;
    case TT_DEPTH_TARGET:
        switch (NewFormat)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
            // Only has the depth flag set
            pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_DEPTH_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 0, 1);
            break;

        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            // Can have depth and stencil flag
            pVulkan->SetImageLayout(RetImage.m_VmaImage.GetVkBuffer(), SetupCmdBuffer, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, ImageInfo.initialLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, (VkPipelineStageFlags)0/*unused param*/, (VkPipelineStageFlags)0/*unused param*/, 0, 1, 0, 1);
            break;

        default:
            LOGE("Unhandled depth format!!!");
            break;

        }
        RetImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        break;
    default:
        assert(0);
        break;
    }

    pVulkan->FinishSetupCommandBuffer(SetupCmdBuffer);

    // ... and an ImageView
    VkImageViewCreateInfo ImageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ImageViewInfo.flags = 0;
    ImageViewInfo.image = RetImage.m_VmaImage.GetVkBuffer();
    ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // <== No support for VK_IMAGE_VIEW_TYPE_CUBE
    ImageViewInfo.format = ImageInfo.format;
    ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // VK_COMPONENT_SWIZZLE_R;
    ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; // VK_COMPONENT_SWIZZLE_G;
    ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // VK_COMPONENT_SWIZZLE_B;
    ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; // VK_COMPONENT_SWIZZLE_A;
    ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageViewInfo.subresourceRange.baseMipLevel = 0;
    ImageViewInfo.subresourceRange.levelCount = 1;  // <== No support for mipmaps
    ImageViewInfo.subresourceRange.baseArrayLayer = 0;
    ImageViewInfo.subresourceRange.layerCount = 1;

    VkSamplerAddressMode SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;   // VK_SAMPLER_ADDRESS_MODE_REPEAT

    switch (TexType)
    {
    case TT_NORMAL:
    case TT_RENDER_TARGET:
    case TT_RENDER_TARGET_SUBPASS:
    case TT_COMPUTE_TARGET:
        ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        break;
    case TT_DEPTH_TARGET:
        switch (NewFormat)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
            // Only has the depth flag set
            ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;

        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            // This is the error from validation layer: "ImageView (XXX) has layout VK_IMAGE_LAYOUT_UNDEFINED 
            // and is using depth/stencil image of format VK_FORMAT_D32_SFLOAT_S8_UINT but it has both STENCIL 
            // and DEPTH aspects set, which is illegal. When using a depth / stencil image in a descriptor set, 
            // please only set either VK_IMAGE_ASPECT_DEPTH_BIT or VK_IMAGE_ASPECT_STENCIL_BIT depending on whether 
            // it will be used for depth reads or stencil reads respectively
            ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;  //  | VK_IMAGE_ASPECT_STENCIL_BIT;
            break;

        default:
            LOGE("Unhandled depth format!!!");
            break;
        }
        SamplerMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        break;

    default:
        assert(0);
        break;
    }

    RetVal = vkCreateImageView(pVulkan->m_VulkanDevice, &ImageViewInfo, NULL, &RetImageView);
    if (!CheckVkError("vkCreateImageView()", RetVal))
    {
        return {};
    }

    // LOGI("vkCreateImageView: %s -> %p", pName, RetImageView);

    // Need a sampler...
    VkSamplerCreateInfo SamplerInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    SamplerInfo.flags = 0;
    SamplerInfo.magFilter = VK_FILTER_LINEAR;
    SamplerInfo.minFilter = VK_FILTER_LINEAR;
    SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;             // TODO: We don't currently support mipmaps
    SamplerInfo.addressModeU = SamplerMode;
    SamplerInfo.addressModeV = SamplerMode;
    SamplerInfo.addressModeW = SamplerMode;
    SamplerInfo.mipLodBias = 0.0f;
    SamplerInfo.anisotropyEnable = VK_FALSE;
    SamplerInfo.maxAnisotropy = 1.0f;
    SamplerInfo.compareEnable = VK_FALSE;
    SamplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    SamplerInfo.minLod = 0.0f;
    SamplerInfo.maxLod = 1.0f;
    SamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;   // VkBorderColor
    SamplerInfo.unnormalizedCoordinates = VK_FALSE;

    RetVal = vkCreateSampler(pVulkan->m_VulkanDevice, &SamplerInfo, NULL, &RetSampler);
    if (!CheckVkError("vkCreateSampler()", RetVal))
    {
        return {};
    }

    VulkanTexInfo RetTex{ uiWidth, uiHeight, 1, NewFormat, RetImageLayout, std::move( RetImage.m_VmaImage ), RetSampler, RetImageView };
    return RetTex;
}
