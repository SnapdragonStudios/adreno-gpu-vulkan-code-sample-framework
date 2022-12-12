//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "frameworkApplicationBase.hpp"

#include "vulkan/vulkan.hpp"
#include "system/os_common.h"
#include "material/shaderModule.hpp"
#include "memory/vertexBufferObject.hpp"
#include "gui/gui.hpp"

// Bring in the timestamp (and assign to a variable)
#include "../../project/buildtimestamp.h"
const char* const FrameworkApplicationBase::sm_BuildTimestamp = BUILD_TIMESTAMP;


//#########################################################
// Config options - Begin
//#########################################################
// ************************************
// General Settings
// ************************************
VAR(uint32_t, gSurfaceWidth, 1280, kVariableNonpersistent);
VAR(uint32_t, gSurfaceHeight, 720, kVariableNonpersistent);

VAR(uint32_t, gRenderWidth, 1280, kVariableNonpersistent);
VAR(uint32_t, gRenderHeight, 720, kVariableNonpersistent);

VAR(uint32_t, gReflectMapWidth, 1280/2, kVariableNonpersistent);
VAR(uint32_t, gReflectMapHeight, 720/2, kVariableNonpersistent);

VAR(uint32_t, gShadowMapWidth, 1024, kVariableNonpersistent);
VAR(uint32_t, gShadowMapHeight, 1024, kVariableNonpersistent);

VAR(uint32_t, gHudRenderWidth, 1280, kVariableNonpersistent);
VAR(uint32_t, gHudRenderHeight, 720, kVariableNonpersistent);

VAR(float,    gFixedFrameRate, 0.0f, kVariableNonpersistent);

VAR(uint32_t, gFramesToRender, 0/*0=loop forever*/, kVariableNonpersistent);
VAR(bool,     gRunOnHLM, false, kVariableNonpersistent);
VAR(int,      gHLMDumpFrame, -1, kVariableNonpersistent);       // start dumping on this frame
VAR(int,      gHLMDumpFrameCount, 1, kVariableNonpersistent );  // dump for this number of frames
VAR(char*,    gHLMDumpFile, "output", kVariableNonpersistent);

VAR(bool,     gFifoPresentMode, false, kVariableNonpersistent); // enable to use FIFO present mode (locks app to refresh rate)


//#########################################################
// Config options - End
//#########################################################

//-----------------------------------------------------------------------------
FrameworkApplicationBase::FrameworkApplicationBase()
//-----------------------------------------------------------------------------
{
    m_vulkan = std::make_unique<Vulkan>();
    m_AssetManager = std::make_unique<AssetManager>();
    m_ConfigFilename = "app_config.txt";

    // FPS and frametime Handling
    m_CurrentFPS = 0.0f;
    m_FpsFrameCount = 0;
    m_FrameCount = 0;
    m_LastUpdateTimeUS = OS_GetTimeUS();
    m_LastFpsCalcTimeMS = (uint32_t) (m_LastUpdateTimeUS / 1000);
    m_LastFpsLogTimeMS = m_LastFpsCalcTimeMS;

    m_WindowWidth = 0;
    m_WindowHeight = 0;

    LOGI("Application build time: %s", sm_BuildTimestamp);
}

//-----------------------------------------------------------------------------
FrameworkApplicationBase::~FrameworkApplicationBase()
//-----------------------------------------------------------------------------
{
    // TODO: Does m_vulkan need any cleanup?
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::SetAndroidAssetManager(AAssetManager* pAAssetManager)
//-----------------------------------------------------------------------------
{
    m_AssetManager->SetAAssetManager(pAAssetManager);
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::SetAndroidExternalFilesDir(const std::string& androidExternalFilesDir)
//-----------------------------------------------------------------------------
{
    m_AssetManager->SetAndroidExternalFilesDir(androidExternalFilesDir);
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::SetConfigFilename(const std::string& filename)
//-----------------------------------------------------------------------------
{
    m_ConfigFilename = filename;
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::LoadConfigFile()
//-----------------------------------------------------------------------------
{
    const std::string ConfigFileFallbackPath = std::string("Media\\") + m_ConfigFilename;
    LOGI("Loading Configuration File: %s", m_ConfigFilename.c_str());
    std::string configFile;
    if (!m_AssetManager->LoadFileIntoMemory(m_ConfigFilename, configFile ) &&
        !m_AssetManager->LoadFileIntoMemory(ConfigFileFallbackPath, configFile))
    {
        LOGE("Error loading Configuration file: %s (and fallback: %s)", m_ConfigFilename.c_str(), ConfigFileFallbackPath.c_str());
    }
    else
    {
        LOGI("Parsing Variable Buffer: ");
        // LogInfo("%s", pConfigFile);
        LoadVariableBuffer(configFile.c_str());
    }

    return true;
}

//-----------------------------------------------------------------------------
int FrameworkApplicationBase::PreInitializeSelectSurfaceFormat(tcb::span<const VkSurfaceFormatKHR>)
//-----------------------------------------------------------------------------
{
    return -1;
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& config)
//-----------------------------------------------------------------------------
{
    if (gFifoPresentMode)
    {
        config.PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    }
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::Initialize(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    gSurfaceWidth = m_vulkan->m_SurfaceWidth; // Sync with the actual swapchain size
    gSurfaceHeight = m_vulkan->m_SurfaceHeight; // Sync with the actual swapchain size

    return true;
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::PostInitialize()
//-----------------------------------------------------------------------------
{
    m_LastUpdateTimeUS = OS_GetTimeUS();
    return true;
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::ReInitialize(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    return true;
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::Destroy()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::SetWindowSize(uint32_t width, uint32_t height)
//-----------------------------------------------------------------------------
{
    LOGI("SetWindowSize: window size %u x %u", width, height);

    m_WindowWidth = width;
    m_WindowHeight = height;

    return true;
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::Render()
//-----------------------------------------------------------------------------
{
    //
    // Gather pre-render timing statistics
    //
    m_FpsFrameCount++;

    uint64_t TimeNowUS = OS_GetTimeUS();
    uint32_t TimeNowMS = (uint32_t) (TimeNowUS / 1000);
    {
        float DiffTime = (float)(TimeNowMS - m_LastFpsCalcTimeMS) * 0.001f;     // Time in seconds
        if (DiffTime > m_FpsEvaluateInterval)
        {
            m_CurrentFPS = (float)m_FpsFrameCount / DiffTime;

            m_FpsFrameCount = 0;
            m_LastFpsCalcTimeMS = TimeNowMS;
        }
    }

    float fltDiffTime = (float)(TimeNowUS - m_LastUpdateTimeUS) * 0.000001f;     // Time in seconds
    m_LastUpdateTimeUS = TimeNowUS;

    if (gFixedFrameRate > 0.0f)
        fltDiffTime = 1.0f / gFixedFrameRate;

    //
    // Call in to the derived application class
    //
    Render(fltDiffTime);

    //
    // Gather post render timing statistics
    //
    if (m_LogFPS)
    {
        // Log FPS is configured
        uint32_t TimeNow = OS_GetTimeMS();
        uint32_t DiffTime = TimeNow - m_LastFpsLogTimeMS;
        if (DiffTime > 1000)
        {
            LOGI("FPS: %0.2f", m_CurrentFPS);
            m_LastFpsLogTimeMS = TimeNow;
        }
    }

    m_FrameCount++;

    //
    // Determine if app should exit (reached requested number of frames)
    //
    if (gFramesToRender != 0 && m_FrameCount >= gFramesToRender)
    {
        // Exiting, the GPU is potentially still running queued jobs, wait for them to finish.
        vkQueueWaitIdle(m_vulkan->m_VulkanQueue);
        return false;   // Exit
    }
    return true;        // Ok
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::KeyDownEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::KeyRepeatEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::KeyUpEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::TouchDownEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::TouchMoveEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::TouchUpEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::InitOneLayout(MaterialProps* pMaterial)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = m_vulkan.get();

    // This layout needs to match one set for vkUpdateDescriptorSets

    // Used as scratch for setting them up below.  Just make large enough to play with
    uint32_t WhichLayout = 0;
    VkDescriptorSetLayoutBinding BindingLayouts[25];
    memset(BindingLayouts, 0, sizeof(BindingLayouts));

    // The only thing that changes on this structure is the number of valid bindings
    VkDescriptorSetLayoutCreateInfo LayoutInfo {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    LayoutInfo.flags = 0;
    LayoutInfo.bindingCount = 0;    // Changed by each instance below
    LayoutInfo.pBindings = BindingLayouts;

    // The only thing that changes on this structure is the pointer to the layout 
    VkPipelineLayoutCreateInfo PipelineLayoutInfo {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    PipelineLayoutInfo.setLayoutCount = 1;  // Should never change from 1
    PipelineLayoutInfo.pSetLayouts = NULL;  // Changed by each instance below

    // Always a Vert Buffer
    BindingLayouts[WhichLayout].binding = SHADER_VERT_UBO_LOCATION;
    BindingLayouts[WhichLayout].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    BindingLayouts[WhichLayout].descriptorCount = 1;
    BindingLayouts[WhichLayout].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    WhichLayout++;

    // Always a Frag Buffer
    BindingLayouts[WhichLayout].binding = SHADER_FRAG_UBO_LOCATION;
    BindingLayouts[WhichLayout].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    BindingLayouts[WhichLayout].descriptorCount = 1;
    BindingLayouts[WhichLayout].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    WhichLayout++;

    // Samplers
    for (uint32_t WhichTexture = 0; WhichTexture < MAX_MATERIAL_SAMPLERS; WhichTexture++)
    {
        if (pMaterial->pTexture[WhichTexture] != nullptr)
        {
            BindingLayouts[WhichLayout].binding = SHADER_BASE_TEXTURE_LOC + WhichTexture;
            BindingLayouts[WhichLayout].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            BindingLayouts[WhichLayout].descriptorCount = 1;
            BindingLayouts[WhichLayout].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            WhichLayout++;
        }
    }

    // How many layouts did we end up with?
    LayoutInfo.bindingCount = WhichLayout;

    // Update the layout info structure and create the layout
    RetVal = vkCreateDescriptorSetLayout(pVulkan->m_VulkanDevice, &LayoutInfo, NULL, &pMaterial->DescLayout);
    if (!CheckVkError("vkCreateDescriptorSetLayout()", RetVal))
    {
        return;
    }

    // Update the other layout structure and create the pipeline layout
    PipelineLayoutInfo.pSetLayouts = &pMaterial->DescLayout;
    RetVal = vkCreatePipelineLayout(pVulkan->m_VulkanDevice, &PipelineLayoutInfo, NULL, &pMaterial->PipelineLayout);
    if (!CheckVkError("vkCreatePipelineLayout()", RetVal))
    {
        return;
    }
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::InitOnePipeline(MaterialProps* pMaterial, MeshObject* pMesh, uint32_t TargetWidth, uint32_t TargetHeight, VkRenderPass RenderPass)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = m_vulkan.get();

    // This is based on a specific mesh at this point
    // TODO: How do I do multiple meshes?

    // Raster State
    VkPipelineRasterizationStateCreateInfo RasterState {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    RasterState.flags = 0;
    RasterState.depthClampEnable = VK_FALSE;
    RasterState.rasterizerDiscardEnable = VK_FALSE;
    RasterState.polygonMode = VK_POLYGON_MODE_FILL;
    RasterState.cullMode = pMaterial->CullMode;
    RasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    RasterState.depthBiasEnable = pMaterial->DepthBiasEnable;

    if (pMaterial->DepthBiasEnable == VK_TRUE)
    {
        RasterState.depthBiasConstantFactor = pMaterial->DepthBiasConstant;
        RasterState.depthBiasClamp = pMaterial->DepthBiasClamp;
        RasterState.depthBiasSlopeFactor = pMaterial->DepthBiasSlope;

        // Sample sets these to 0 and then lets dynamic state handle it
        RasterState.depthBiasConstantFactor = 0.0f;
        RasterState.depthBiasClamp = 0.0f;
        RasterState.depthBiasSlopeFactor = 0.0f;
    }
    else
    {
        RasterState.depthBiasConstantFactor = 0.0f;
        RasterState.depthBiasClamp = 0.0f;
        RasterState.depthBiasSlopeFactor = 0.0f;
    }

    RasterState.lineWidth = 1.0f;

    // Blend State (Needs to be an array)

    // TODO: Set up actual blending
    VkPipelineColorBlendAttachmentState BlendStates[MAX_GMEM_OUTPUT_LAYERS];
    memset(BlendStates, 0, sizeof(BlendStates));

    if (pMaterial->NumBlendStates > MAX_GMEM_OUTPUT_LAYERS)
    {
        LOGE("Material sets more than %d blend states!", pMaterial->NumBlendStates);
        return false;
    }
    for (uint32_t WhichBlendState = 0; WhichBlendState < pMaterial->NumBlendStates; WhichBlendState++)
    {
        // Simply assign the whole structure over
        BlendStates[WhichBlendState] = pMaterial->BlendStates[WhichBlendState];

        // Overwrite the color write mask to be sure
        BlendStates[WhichBlendState].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }   // Each Blend State

    VkPipelineColorBlendStateCreateInfo BlendStateInfo {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    BlendStateInfo.flags = 0;
    BlendStateInfo.logicOpEnable = VK_FALSE;
    // BlendStateInfo.logicOp;
    BlendStateInfo.attachmentCount = pMaterial->NumBlendStates;
    BlendStateInfo.pAttachments = BlendStates;
    // BlendStateInfo.blendConstants[4];

    // Viewport and Scissor
    // TODO: Handle rendering into RT
    VkViewport Viewport {};
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = (float)TargetWidth;
    Viewport.height = (float)TargetHeight;
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;

    VkRect2D Scissor {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = TargetWidth;
    Scissor.extent.height = TargetHeight;

    // Depth and Stencil
    VkPipelineDepthStencilStateCreateInfo DepthStencilInfo {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    DepthStencilInfo.flags = 0;
    DepthStencilInfo.depthTestEnable = pMaterial->DepthTestEnable;
    DepthStencilInfo.depthWriteEnable = pMaterial->DepthWriteEnable;
    DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    DepthStencilInfo.stencilTestEnable = VK_FALSE;

    DepthStencilInfo.front.failOp = VK_STENCIL_OP_KEEP;
    DepthStencilInfo.front.passOp = VK_STENCIL_OP_KEEP;
    DepthStencilInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
    DepthStencilInfo.front.compareOp = VK_COMPARE_OP_NEVER;
    DepthStencilInfo.front.compareMask = 0;
    DepthStencilInfo.front.writeMask = 0;
    DepthStencilInfo.front.reference = 0;

    DepthStencilInfo.back.failOp = VK_STENCIL_OP_KEEP;
    DepthStencilInfo.back.passOp = VK_STENCIL_OP_KEEP;
    DepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
    DepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
    DepthStencilInfo.back.compareMask = 0;
    DepthStencilInfo.back.writeMask = 0;
    DepthStencilInfo.back.reference = 0;

    DepthStencilInfo.minDepthBounds = 0.0f;
    DepthStencilInfo.maxDepthBounds = 0.0f;

    // Setup any dynamic states
    std::vector<VkDynamicState> dynamicStateEnables;
    
    if (pMaterial->DepthBiasEnable == VK_TRUE)
        dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);

    // Grab the vertex input state for the vertex buffer we will be binding.
    VkPipelineVertexInputStateCreateInfo visci = pMesh->m_VertexBuffers[0].CreatePipelineState();

    if( !pVulkan->CreatePipeline( pMaterial->PipelineCache,
                                  &visci,
                                  pMaterial->PipelineLayout,
                                  RenderPass,
                                  0,//subpass,
                                  &RasterState,
                                  &DepthStencilInfo,
                                  &BlendStateInfo,
                                  nullptr,//default multisample state
                                  dynamicStateEnables,
                                  &Viewport,
                                  &Scissor,
                                  pMaterial->pShader->VertShaderModule.GetVkShaderModule(),
                                  pMaterial->pShader->FragShaderModule.GetVkShaderModule(),
                                  false, VK_NULL_HANDLE,
                                  &pMaterial->Pipeline ) )
    {
        LOGE( "CreatePipeline error" );
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::InitDescriptorPool(MaterialProps* pMaterial)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = m_vulkan.get();

    // Used as scratch for setting them up below.  Just make large enough to play with
    uint32_t PoolCount = 0;
    VkDescriptorPoolSize PoolSize[10];
    memset(PoolSize, 0, sizeof(PoolSize));

    // Need uniform buffers for Vert and Frag shaders
    PoolSize[PoolCount].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    PoolSize[PoolCount].descriptorCount = NUM_VULKAN_BUFFERS * 2;    // Need for each buffer (Vert + Frag) * NUM_VULKAN_BUFFERS
    PoolCount++;

    // Need combined image samplers for each possible texture
    PoolSize[PoolCount].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    PoolSize[PoolCount].descriptorCount = NUM_VULKAN_BUFFERS * MAX_MATERIAL_SAMPLERS;    // Need for each buffer
    PoolCount++;

    VkDescriptorPoolCreateInfo PoolInfo;
    memset(&PoolInfo, 0, sizeof(VkDescriptorPoolCreateInfo));
    PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    PoolInfo.pNext = NULL;
    PoolInfo.flags = 0;     // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to allow them to be returned
    PoolInfo.maxSets = NUM_VULKAN_BUFFERS;  // Since descriptor sets come out of this pool we need more than one
    PoolInfo.poolSizeCount = PoolCount;
    PoolInfo.pPoolSizes = PoolSize;

    RetVal = vkCreateDescriptorPool(pVulkan->m_VulkanDevice, &PoolInfo, NULL, &pMaterial->DescPool);
    if (!CheckVkError("vkCreateDescriptorPool()", RetVal))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool FrameworkApplicationBase::InitDescriptorSet(MaterialProps* pMaterial)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = m_vulkan.get();

    // Need Descriptor Buffers
    VkDescriptorBufferInfo VertDescBufferInfo;
    memset(&VertDescBufferInfo, 0, sizeof(VkDescriptorBufferInfo));
    VertDescBufferInfo.buffer = pMaterial->pVertUniform->bufferInfo.buffer;
    if (pMaterial->VertUniformLength == 0)
    {
        // This means it has not been set
        if (pMaterial->VertUniformOffset != 0)
        {
            LOGE("DescriptorSet defines vert uniform buffer offset but the length is 0!");
        }
        VertDescBufferInfo.offset = pMaterial->pVertUniform->bufferInfo.offset;
        VertDescBufferInfo.range = pMaterial->pVertUniform->bufferInfo.range;
    }
    else
    {
        VertDescBufferInfo.offset = pMaterial->VertUniformOffset;
        VertDescBufferInfo.range = pMaterial->VertUniformLength;
    }

    VkDescriptorBufferInfo FragDescBufferInfo;
    memset(&FragDescBufferInfo, 0, sizeof(VkDescriptorBufferInfo));
    FragDescBufferInfo.buffer = pMaterial->pFragUniform->bufferInfo.buffer;
    if (pMaterial->FragUniformLength == 0)
    {
        // This means it has not been set
        if (pMaterial->FragUniformOffset != 0)
        {
            LOGE("DescriptorSet defines frag uniform buffer offset but the length is 0!");
        }
        FragDescBufferInfo.offset = pMaterial->pFragUniform->bufferInfo.offset;
        FragDescBufferInfo.range = pMaterial->pFragUniform->bufferInfo.range;
    }
    else
    {
        FragDescBufferInfo.offset = pMaterial->FragUniformOffset;
        FragDescBufferInfo.range = pMaterial->FragUniformLength;
    }

    VkDescriptorSetAllocateInfo DescSetInfo;
    memset(&DescSetInfo, 0, sizeof(VkDescriptorSetAllocateInfo));
    DescSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescSetInfo.pNext = NULL;
    DescSetInfo.descriptorPool = pMaterial->DescPool;
    DescSetInfo.descriptorSetCount = 1;
    DescSetInfo.pSetLayouts = &pMaterial->DescLayout;

    RetVal = vkAllocateDescriptorSets(pVulkan->m_VulkanDevice, &DescSetInfo, &pMaterial->DescSet);
    if (!CheckVkError("vkAllocateDescriptorSets()", RetVal))
    {
        return false;
    }

    // Used as scratch for setting them up below.  Just make large enough to play with
    uint32_t DescCount = 0;

    VkDescriptorImageInfo DescImageInfo[25];
    memset(DescImageInfo, 0, sizeof(DescImageInfo));

    VkWriteDescriptorSet WriteInfo[25];
    memset(&WriteInfo, 0, sizeof(WriteInfo));


    // Always a Vert Buffer
    WriteInfo[DescCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    WriteInfo[DescCount].pNext = NULL;
    WriteInfo[DescCount].dstSet = pMaterial->DescSet;
    WriteInfo[DescCount].dstBinding = SHADER_VERT_UBO_LOCATION;
    WriteInfo[DescCount].dstArrayElement = 0;
    WriteInfo[DescCount].descriptorCount = 1;
    WriteInfo[DescCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;    // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    WriteInfo[DescCount].pImageInfo = NULL;
    WriteInfo[DescCount].pBufferInfo = &VertDescBufferInfo;
    WriteInfo[DescCount].pTexelBufferView = NULL;
    DescCount++;

    // Always a Frag Buffer
    WriteInfo[DescCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    WriteInfo[DescCount].pNext = NULL;
    WriteInfo[DescCount].dstSet = pMaterial->DescSet;
    WriteInfo[DescCount].dstBinding = SHADER_FRAG_UBO_LOCATION;
    WriteInfo[DescCount].dstArrayElement = 0;
    WriteInfo[DescCount].descriptorCount = 1;
    WriteInfo[DescCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;    // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    WriteInfo[DescCount].pImageInfo = NULL;
    WriteInfo[DescCount].pBufferInfo = &FragDescBufferInfo;
    WriteInfo[DescCount].pTexelBufferView = NULL;
    DescCount++;

    // Samplers
    for (uint32_t WhichTexture = 0; WhichTexture < MAX_MATERIAL_SAMPLERS; WhichTexture++)
    {
        if (pMaterial->pTexture[WhichTexture] != nullptr)
        {
            DescImageInfo[DescCount] = pMaterial->pTexture[WhichTexture]->GetVkDescriptorImageInfo();

            WriteInfo[DescCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            WriteInfo[DescCount].pNext = NULL;
            WriteInfo[DescCount].dstSet = pMaterial->DescSet;
            WriteInfo[DescCount].dstBinding = SHADER_BASE_TEXTURE_LOC + WhichTexture;
            WriteInfo[DescCount].dstArrayElement = 0;
            WriteInfo[DescCount].descriptorCount = 1;
            WriteInfo[DescCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            WriteInfo[DescCount].pImageInfo = &DescImageInfo[DescCount];
            WriteInfo[DescCount].pBufferInfo = NULL;
            WriteInfo[DescCount].pTexelBufferView = NULL;
            DescCount++;
        }
    }

    vkUpdateDescriptorSets(pVulkan->m_VulkanDevice, DescCount, WriteInfo, 0, NULL);

    return true;
}

//-----------------------------------------------------------------------------
void FrameworkApplicationBase::SetOneImageLayout(VkImage WhichImage,
    VkImageAspectFlags aspect,
    VkImageLayout oldLayout,
    VkImageLayout newLayout)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    Vulkan* pVulkan = m_vulkan.get();

    VkCommandBuffer LocalCmdBuffer = pVulkan->StartSetupCommandBuffer();

    VkPipelineStageFlags srcMask = 0;
    VkPipelineStageFlags dstMask = 1;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = 1;
    uint32_t baseLayer = 0;
    uint32_t layerCount = 1;

    pVulkan->SetImageLayout(WhichImage,
        LocalCmdBuffer,
        aspect,
        oldLayout, // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        newLayout,
        srcMask,
        dstMask,
        baseMipLevel,
        mipLevelCount,
        baseLayer,
        layerCount);

    pVulkan->FinishSetupCommandBuffer(LocalCmdBuffer);
}


