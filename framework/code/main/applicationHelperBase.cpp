//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "memory/vulkan/indexBufferObject.hpp"
#include "memory/vulkan/vertexBufferObject.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "material/computable.hpp"
#include "material/drawable.hpp"
#include "material/vertexFormat.hpp"
#include "material/materialManager.hpp"
#include "material/vulkan/specializationConstantsLayout.hpp"
#include "material/shaderManagerT.hpp"
#include "mesh/meshHelper.hpp"
#include "texture/vulkan/texture.hpp"
#include "texture/vulkan/loaderKtx.hpp"
#include "texture/vulkan/textureManager.hpp"
#include "vulkan/commandBuffer.hpp"
#include "vulkan/renderTarget.hpp"
#include "vulkan/timerPool.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_support.hpp"
#if VK_KHR_ray_tracing_pipeline
#include "vulkanRT/traceable.hpp"
#endif // VK_KHR_ray_tracing_pipeline
#include "applicationHelperBase.hpp"    // last header to be included

extern "C" {
VAR(float, gCameraRotateSpeed, 0.25f, kVariableNonpersistent);
VAR(float, gCameraMoveSpeed, 4.0f, kVariableNonpersistent);
}; //extern "C"


//-----------------------------------------------------------------------------
ApplicationHelperBase::ApplicationHelperBase() noexcept : FrameworkApplicationBase()
//-----------------------------------------------------------------------------
{
    m_gfxBase = std::make_unique<Vulkan>();
}

//-----------------------------------------------------------------------------
ApplicationHelperBase::~ApplicationHelperBase()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool ApplicationHelperBase::Initialize(uintptr_t hWnd, uintptr_t hInstance)
//-----------------------------------------------------------------------------
{
    if (!FrameworkApplicationBase::Initialize(hWnd, hInstance))
        return false;

    auto* const pVulkan = GetVulkan();

    if (!pVulkan || !pVulkan->Init(hWnd, hInstance,
                                   [this](std::span<const SurfaceFormat> x) { return PreInitializeSelectSurfaceFormat(x); },
                                   [this](Vulkan::AppConfiguration& x) { return PreInitializeSetVulkanConfiguration(x); }))
    {
        LOGE("Unable to initialize Vulkan!!");
        return false;
    }

    CRenderTargetArray<NUM_VULKAN_BUFFERS> backbuffer;
    if (!m_BackbufferRenderTarget.InitializeFromSwapchain(pVulkan))
        return false;

    auto textureManagerVulkan = std::make_unique<TextureManagerVulkan>(*pVulkan);
    if (!textureManagerVulkan->Initialize())
        return false;
    m_TextureManager = std::move(textureManagerVulkan);

    m_SamplerRepeat = CreateSampler(*pVulkan, SamplerAddressMode::Repeat, SamplerFilter::Linear, SamplerBorderColor::TransparentBlackFloat, 0.0f/*mipBias*/);
    if (m_SamplerRepeat.IsEmpty())
        return false;
    m_SamplerEdgeClamp = CreateSampler(*pVulkan, SamplerAddressMode::ClampEdge, SamplerFilter::Linear, SamplerBorderColor::TransparentBlackFloat, 0.0f/*mipBias*/);
    if (m_SamplerEdgeClamp.IsEmpty())
        return false;
    m_SamplerMirroredRepeat = CreateSampler(*pVulkan, SamplerAddressMode::MirroredRepeat, SamplerFilter::Linear, SamplerBorderColor::TransparentBlackFloat, 0.0f/*mipBias*/);
    if (m_SamplerMirroredRepeat.IsEmpty())
        return false;

    m_ShaderManager = std::make_unique<ShaderManagerT<tGfxApi>>(*pVulkan);

    m_MaterialManager = std::make_unique<MaterialManagerT<tGfxApi>>();

    return true;
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::Destroy()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    vkDestroySampler(pVulkan->m_VulkanDevice, m_SamplerMirroredRepeat.GetVkSampler(), nullptr);
    //m_SamplerMirroredRepeat = VK_NULL_HANDLE;
    vkDestroySampler(pVulkan->m_VulkanDevice, m_SamplerEdgeClamp.GetVkSampler(), nullptr);
    //m_SamplerEdgeClamp = VK_NULL_HANDLE;
    vkDestroySampler(pVulkan->m_VulkanDevice, m_SamplerRepeat.GetVkSampler(), nullptr);
    //m_SamplerRepeat = VK_NULL_HANDLE;

    m_TextureManager.reset();
    m_MaterialManager.reset();
    m_ShaderManager.reset();

    //m_BackbufferRenderTarget.HardReset();   // DO NOT destroy as this instance does not own its framebuffer (points to the vulkan backbuffers)
    FrameworkApplicationBase::Destroy();
}


//-----------------------------------------------------------------------------
bool ApplicationHelperBase::SetWindowSize(uint32_t width, uint32_t height)
//-----------------------------------------------------------------------------
{
    if (!FrameworkApplicationBase::SetWindowSize(width, height))
        return false;

    m_Camera.SetAspect(float(width) / float(height));
    if (m_CameraController)
    {
        m_CameraController->SetSize(width, height);
    }
    return true;
}

//-----------------------------------------------------------------------------
int ApplicationHelperBase::PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat>)
//-----------------------------------------------------------------------------
{
    return -1;
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration&)
//-----------------------------------------------------------------------------
{}

//-----------------------------------------------------------------------------
bool ApplicationHelperBase::InitCamera()
//-----------------------------------------------------------------------------
{
    // Camera
    m_Camera.SetAspect(float(gRenderWidth) / float(gRenderHeight));

    // Camera Controller

#if defined(OS_ANDROID) ///TODO: make this an option
    typedef CameraControllerTouch           tCameraController;
#else
    typedef CameraController                tCameraController;
#endif

    auto cameraController = std::make_unique<tCameraController>();
    if (!cameraController->Initialize(m_WindowWidth, m_WindowHeight))
    {
        return false;
    }
    cameraController->SetRotateSpeed(gCameraRotateSpeed);
    cameraController->SetMoveSpeed(gCameraMoveSpeed);
    m_CameraController = std::move(cameraController);
    return true;
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::AddDrawableToCmdBuffers(const Drawable& drawable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numRenderPasses, uint32_t numVulkanBuffers, uint32_t startDescriptorSetIdx) const
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = GetVulkan();

    const auto& drawablePasses = drawable.GetDrawablePasses();
    for (const auto& drawablePass : drawablePasses)
    {
        const auto passIdx = drawablePass.mPassIdx;
        assert(passIdx < numRenderPasses);

        for (uint32_t bufferIdx = 0; bufferIdx < numVulkanBuffers; ++bufferIdx)
        {
            Wrap_VkCommandBuffer& buffer = cmdBuffers[bufferIdx * numRenderPasses + passIdx];
            VkCommandBuffer cmdBuffer = buffer.m_VkCommandBuffer;
            assert(cmdBuffer != VK_NULL_HANDLE);

            // Add commands to bind the pipeline, buffers etc and issue the draw.
            drawable.DrawPass(cmdBuffer, drawablePass, drawablePass.mDescriptorSet.empty() ? 0 : (startDescriptorSetIdx + bufferIdx) % drawablePass.mDescriptorSet.size() );

            ++buffer.m_NumDrawCalls;
            buffer.m_NumTriangles += drawablePass.mNumVertices / 3;
        }
    }
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::AddComputableToCmdBuffer(const Computable& computable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numCmdBuffers, uint32_t startDescriptorSetIdx, TimerPoolBase* timerPool) const
//-----------------------------------------------------------------------------
{
    // LOGI("AddComputableToCmdBuffer() Entered...");

    assert(cmdBuffers != nullptr);

    for(uint32_t whichBuffer = 0; whichBuffer < numCmdBuffers; ++whichBuffer)
    {
        for (uint32_t passIdx =0; const auto& computablePass : computable.GetPasses())
        {
            const int timerIdx = timerPool ? cmdBuffers->StartGpuTimer( computable.GetPassName( passIdx )) : -1;
            computable.DispatchPass(cmdBuffers->m_VkCommandBuffer, computablePass, (whichBuffer + startDescriptorSetIdx) % (uint32_t)computablePass.GetVkDescriptorSets().size());
            if (timerIdx>=0)
                cmdBuffers->StopGpuTimer( timerIdx );
            ++passIdx;
        }
        ++cmdBuffers;
    }
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::AddComputableOutputBarrierToCmdBuffer(const Computable& computable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numCmdBuffers) const
//-----------------------------------------------------------------------------
{
    const auto& computableOutputBufferBarriers = computable.GetBufferOutputMemoryBarriers();
    const auto& computableOutputImageBarriers = computable.GetImageOutputMemoryBarriers();

    if (computableOutputBufferBarriers.empty() && computableOutputImageBarriers.empty())
        return;

    // Barrier on memory, with correct layouts set.
    for (uint32_t WhichBuffer = 0; WhichBuffer < numCmdBuffers; ++WhichBuffer)
    {
        vkCmdPipelineBarrier(cmdBuffers[WhichBuffer].m_VkCommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // srcMask,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // dstMask,
                             0,
                             0, nullptr,
                             (uint32_t)computableOutputBufferBarriers.size(),
                             computableOutputBufferBarriers.empty() ? nullptr : computableOutputBufferBarriers.data(),
                             (uint32_t)computableOutputImageBarriers.size(),
                             computableOutputImageBarriers.empty() ? nullptr : computableOutputImageBarriers.data());
    }
}

#if VK_KHR_ray_tracing_pipeline
//-----------------------------------------------------------------------------
void ApplicationHelperBase::AddTraceableToCmdBuffer(const Traceable& traceable, Wrap_VkCommandBuffer* cmdBuffers, uint32_t numCmdBuffers, uint32_t startDescriptorSetIdx, TimerPoolBase* timerPool) const
//-----------------------------------------------------------------------------
{
    // LOGI("AddTraceableToCmdBuffer() Entered...");

    assert(cmdBuffers != nullptr);

    for (uint32_t whichBuffer = 0; whichBuffer < numCmdBuffers; ++whichBuffer)
    {
        for (uint32_t passIdx = 0; const auto& traceablePass : traceable.GetPasses())
        {
            const int timerIdx = timerPool ? cmdBuffers->StartGpuTimer( traceable.GetPassName( passIdx ) ) : -1;
            traceable.DispatchPass(cmdBuffers->m_VkCommandBuffer, traceablePass, (whichBuffer + startDescriptorSetIdx) % (uint32_t)traceablePass.GetVkDescriptorSets().size());
            if (timerIdx >= 0)
                cmdBuffers->StopGpuTimer( timerIdx );
            ++passIdx;
        }
        ++cmdBuffers;
    }
}
#endif // VK_KHR_ray_tracing_pipeline

//-----------------------------------------------------------------------------
bool ApplicationHelperBase::PresentQueue( const std::span<const VkSemaphore> WaitSemaphores, uint32_t SwapchainPresentIndx )
//-----------------------------------------------------------------------------
{
    auto& vulkan = *GetVulkan();

    if (!vulkan.PresentQueue( WaitSemaphores, SwapchainPresentIndx ))
        return false;

#if OS_WINDOWS
    {
        static int gHLMFrameNumber = -1;

        gHLMFrameNumber++;
        if (gHLMFrameNumber >= gHLMDumpFrame && gHLMFrameNumber < gHLMDumpFrame + gHLMDumpFrameCount && gHLMDumpFile!=nullptr && *gHLMDumpFile!='\0')
        {
            bool dumpResult = DumpImagePixelData(
                vulkan,
                vulkan.GetSwapchainImage(SwapchainPresentIndx),
                vulkan.GetSurfaceFormat(), 
                vulkan.GetSurfaceWidth(),
                vulkan.GetSurfaceHeight(), 
                true, 
                0, 
                0, 
                []( uint32_t width, uint32_t height, TextureFormat format, uint32_t spanBytes, const void* data ) 
                {
                    auto* dataBytes = static_cast<const uint8_t*>(data);

                    std::string fullFileName( gHLMDumpFile );
                    const std::string cBmp( ".bmp" );

                    size_t extPos = fullFileName.rfind( '.' );
                    if (extPos == -1)
                    {
                        extPos = fullFileName.size();
                        fullFileName.append( cBmp );
                    }
                    if (gHLMDumpFrameCount > 1)
                    {
                        // Number the frames (if asking for more than one frame to be saved)
                        const std::string frameIndex( std::to_string( gHLMFrameNumber ) );
                        fullFileName.insert( extPos, frameIndex );
                        extPos += frameIndex.size();
                    }
                    if (extPos != -1 && !std::equal( std::begin( fullFileName ) + extPos, std::end( fullFileName ), std::begin( cBmp ), std::end( cBmp ), []( auto a, auto b )->bool { return std::tolower( a ) == std::tolower( b ); } ))
                    {
                        // save a non BMP image
                        SaveTextureData( fullFileName.c_str(), format, width, height, data );
                    }
                    else
                    {
    #pragma pack(push,2)
                        struct bmp_file_header {
                            uint16_t bfType;
                            uint32_t bfSize;
                            uint16_t bfReserved1;
                            uint16_t bfReserved2;
                            uint32_t bfOffBits;
                        };
    #pragma pack(pop)
                        struct bmp_v4_info_header {
                            uint32_t biSize;
                            int32_t  biWidth;
                            int32_t  biHeight;
                            uint16_t biPlanes;
                            uint16_t biBitCount;
                            uint32_t biCompression;
                            uint32_t biSizeImage;
                            int32_t  biXPelsPerMeter;
                            int32_t  biYPelsPerMeter;
                            uint32_t biClrUsed;
                            uint32_t biClrImportant;
                            uint32_t biRedMask;
                            uint32_t biGreenMask;
                            uint32_t biBlueMask;
                            uint32_t biAlphaMask;
                            uint32_t biCSType;
                            uint32_t biEndPoints[9];
                            uint32_t biGammaRed;
                            uint32_t biGammaGreen;
                            uint32_t biGammaBlue;
                        };

                        FILE* stream;
                        struct bmp_file_header bmf;
                        struct bmp_v4_info_header bmi;

                        memset( &bmf, 0, sizeof( bmf ) );
                        bmf.bfType = 0x4d42;
                        bmf.bfSize = sizeof( bmf ) + sizeof( bmi ) + (height * width * 4);
                        bmf.bfOffBits = sizeof( bmf ) + sizeof( bmi );

                        memset( &bmi, 0, sizeof( bmi ) );
                        bmi.biSize = sizeof( bmi );
                        bmi.biWidth = width;
                        bmi.biHeight = /*-(int32_t)*/height;
                        bmi.biPlanes = 1;
                        bmi.biBitCount = 32;
                        bmi.biCompression = BI_BITFIELDS;
                        bmi.biSizeImage = 0;
                        bmi.biRedMask = 0xff;
                        bmi.biGreenMask = 0xff00;
                        bmi.biBlueMask = 0xff0000;
                        bmi.biAlphaMask = 0xff00000;

                        stream = fopen( fullFileName.c_str(), "wb" );
                        assert( stream );

                        fwrite( &bmf, sizeof( bmf ), 1, stream );
                        fwrite( &bmi, sizeof( bmi ), 1, stream );

                        for (int y = height - 1; y >= 0; y--) {
                            fwrite( dataBytes + spanBytes * y, width * 4, 1, stream );
                        }
                        fclose( stream );
                    }
                } );
            if (!dumpResult)
            {
                return false;
            }
        }
    }
#endif // OS_WINDOWS

    return true;
}

//-----------------------------------------------------------------------------
bool ApplicationHelperBase::PresentQueue(const std::span<const SemaphoreWait> WaitSemaphores, uint32_t SwapchainPresentIndx)
//-----------------------------------------------------------------------------
{
    constexpr uint32_t cMaxPresentSemaphores = 8;
    std::array<VkSemaphore, cMaxPresentSemaphores> semaphores;
    assert(WaitSemaphores.size() < semaphores.size());
    uint32_t numSemaphores = 0;

    for (auto& waitSemaphore : WaitSemaphores)
        semaphores[numSemaphores++] = waitSemaphore.semaphore.GetVkSemaphore();

    return PresentQueue( std::span(semaphores).subspan(0, numSemaphores), SwapchainPresentIndx);
}

bool ApplicationHelperBase::PresentQueue(const SemaphoreWait& WaitSemaphore, uint32_t SwapchainPresentIndx)
{
    auto WaitSemaphores = std::span(&WaitSemaphore, 1);
    return PresentQueue(WaitSemaphores, SwapchainPresentIndx);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::KeyDownEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
    if (m_CameraController)
        m_CameraController->KeyDownEvent(key);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::KeyRepeatEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::KeyUpEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
    if (m_CameraController)
        m_CameraController->KeyUpEvent(key);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::TouchDownEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    // Make sure we are big enough for this ID
    m_TouchStates.resize(std::max(m_TouchStates.size(), (size_t)(iPointerID + 1)), {});

    m_TouchStates[iPointerID].m_isDown = true;
    m_TouchStates[iPointerID].m_xPos = xPos;
    m_TouchStates[iPointerID].m_yPos = yPos;
    m_TouchStates[iPointerID].m_xDownPos = xPos;
    m_TouchStates[iPointerID].m_yDownPos = yPos;

    if (m_CameraController)
        m_CameraController->TouchDownEvent(iPointerID, xPos, yPos);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::TouchMoveEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    // Make sure we are big enough for this ID
    while (m_TouchStates.size() < iPointerID + 1)
    {
        TouchStatus NewEntry;
        m_TouchStates.push_back(NewEntry);
    }

    m_TouchStates[iPointerID].m_isDown = true;
    m_TouchStates[iPointerID].m_xPos = xPos;
    m_TouchStates[iPointerID].m_yPos = yPos;

    if (m_CameraController)
        m_CameraController->TouchMoveEvent(iPointerID, xPos, yPos);
}

//-----------------------------------------------------------------------------
void ApplicationHelperBase::TouchUpEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    // Make sure we are big enough for this ID
    while (m_TouchStates.size() < iPointerID + 1)
    {
        TouchStatus NewEntry;
        m_TouchStates.push_back(NewEntry);
    }

    m_TouchStates[iPointerID].m_isDown = false;
    m_TouchStates[iPointerID].m_xPos = xPos;
    m_TouchStates[iPointerID].m_yPos = yPos;

    if (m_CameraController)
        m_CameraController->TouchUpEvent(iPointerID, xPos, yPos);
}

//-----------------------------------------------------------------------------
TextureT<Vulkan> ApplicationHelperBase::LoadKTXTexture(Vulkan* vulkan, AssetManager& assetManager, const char* filename, SamplerAddressMode samplerMode)
//-----------------------------------------------------------------------------
{
    const Sampler* pSampler = nullptr;
    switch (samplerMode) {
    case SamplerAddressMode::Repeat:
        pSampler = &m_SamplerRepeat;
        break;
    case SamplerAddressMode::ClampEdge:
        pSampler = &m_SamplerEdgeClamp;
        break;
    case SamplerAddressMode::MirroredRepeat:
        pSampler = &m_SamplerMirroredRepeat;
        break;
    default:
        assert(0 && "Invalid sampler");
        break;
    }
    return LoadKTXTexture(vulkan, assetManager, filename, *pSampler);
}

//-----------------------------------------------------------------------------
TextureT<Vulkan> ApplicationHelperBase::LoadKTXTexture(Vulkan* vulkan, AssetManager& assetManager, const char* filename, const Sampler& sampler)
//-----------------------------------------------------------------------------
{
    auto* pTextureManagerVulkan = apiCast<Vulkan>(m_TextureManager.get());
    const auto & samplerVulkan = static_cast<const SamplerT<Vulkan>&>(sampler);
    return pTextureManagerVulkan->GetLoader()->LoadKtx(*vulkan, assetManager, filename, samplerVulkan);
}

//-----------------------------------------------------------------------------
// Loads a .gltf file and builds a single Vertex Buffer containing relevant vertex.
// DOES not use (or populate) the index buffer although it does respect the gltf index buffer when loading the mesh
// positions, normals and threadColors. Vertices are held in TRIANGLE_LIST format.
//
// Vertex layout corresponds to ApplicationHelperBase::vertex_layout structure.
bool ApplicationHelperBase::LoadGLTF(const std::string& filename, uint32_t binding, Mesh<Vulkan>* meshObject)
{
    const auto meshObjects = MeshObjectIntermediate::LoadGLTF(*m_AssetManager, filename);
    if (meshObjects.empty())
        return false;

    return MeshHelper::CreateMesh<Vulkan>(GetVulkan()->GetMemoryManager(), meshObjects[0].CopyFlattened()/*remove indices*/, binding, { &MeshHelper::vertex_layout::sFormat, 1 }, meshObject);
}
