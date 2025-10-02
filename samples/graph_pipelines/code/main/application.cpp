//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "application.hpp"
#include "vulkan/extensionHelpers.hpp"
#include "main/applicationEntrypoint.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "camera/cameraData.hpp"
#include "camera/cameraGltfLoader.hpp"
#include "gui/imguiVulkan.hpp"
#include "material/drawable.hpp"
#include "material/vulkan/shaderModule.hpp"
#include "material/shaderManagerT.hpp"
#include "material/materialManager.hpp"
#include "material/vulkan/specializationConstantsLayout.hpp"
#include "mesh/meshHelper.hpp"
#include "mesh/meshLoader.hpp"
#include "system/math_common.hpp"
#include "texture/textureManager.hpp"
#include "imgui.h"

#include <random>
#include <iostream>
#include <filesystem>
#include <fstream>

/*
* # Major Keypoints:
* 
* - For the Data Graph queue selection, see "Vulkan::InitDataGraph()".
* - For the Data Graph Processing Engine selection, see "bool Vulkan::GetDataGraphProcessingEngine()".
* - For command pool initialization, see "bool Vulkan::InitCommandPools()".
* 
* 
* 
*/

namespace
{
    static constexpr std::array<const char*, NUM_RENDER_PASSES> sRenderPassNames = { "RP_SCENE", "RP_HUD", "RP_BLIT" };

    glm::vec3 gCameraStartPos = glm::vec3(26.48f, 20.0f, -5.21f);
    glm::vec3 gCameraStartRot = glm::vec3(0.0f, 110.0f, 0.0f);

    float   gFOV = PI_DIV_4;
    float   gNearPlane = 1.0f;
    float   gFarPlane = 1800.0f;
    float   gNormalAmount = 0.3f;
    float   gNormalMirrorReflectAmount = 0.05f;

    const char* gModelAssetPath = "Media\\Models\\PipelineCache.bin";
    const char* gMuseumAssetPath = "Media\\Meshes\\Museum.gltf";
    const char* gTextureFolder    = "Media\\Textures\\";

    static uint32_t FindMemoryType(VkPhysicalDevice& physicalDevice, uint32_t type_bits, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((memProperties.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
                return i;
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    };
}

///
/// @brief Implementation of the Application entrypoint (called by the framework)
/// @return Pointer to Application (derived from @FrameworkApplicationBase).
/// Creates the Application class.  Ownership is passed to the calling (framework) function.
/// 
FrameworkApplicationBase* Application_ConstructApplication()
{
    return new Application();
}

Application::Application() : ApplicationHelperBase()
{
}

Application::~Application()
{
}

//-----------------------------------------------------------------------------
void Application::PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& config)
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::PreInitializeSetVulkanConfiguration(config);
    config.RequiredExtension<ExtensionHelper::Ext_VK_KHR_synchronization2>();
    config.RequiredExtension<ExtensionHelper::Ext_VK_KHR_create_renderpass2>();
    config.RequiredExtension<ExtensionHelper::Ext_VK_KHR_get_physical_device_properties2>();

    config.OptionalExtension<ExtensionHelper::Ext_VK_ARM_tensors>();
    config.OptionalExtension<ExtensionHelper::Ext_VK_ARM_data_graph>();
}

//-----------------------------------------------------------------------------
bool Application::Initialize(uintptr_t windowHandle, uintptr_t hInstance)
//-----------------------------------------------------------------------------
{
    if (!ApplicationHelperBase::Initialize( windowHandle, hInstance ))
    {
        return false;
    }

    m_IsGraphPipelinesSupported &= GetVulkan()->HasLoadedVulkanDeviceExtension(VK_ARM_TENSORS_EXTENSION_NAME)
        && GetVulkan()->HasLoadedVulkanDeviceExtension(VK_ARM_DATA_GRAPH_EXTENSION_NAME);
        
    // TODO: Remove when supported by the driver
#if defined(OS_ANDROID)
    {
        auto* Ext_VK_ARM_tensors = static_cast<ExtensionHelper::Ext_VK_ARM_tensors*>(GetVulkan()->m_DeviceExtensions.GetExtension(VK_ARM_TENSORS_EXTENSION_NAME));
        auto* Ext_VK_ARM_data_graph = static_cast<ExtensionHelper::Ext_VK_ARM_data_graph*>(GetVulkan()->m_DeviceExtensions.GetExtension(VK_ARM_DATA_GRAPH_EXTENSION_NAME));
        auto fpGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(GetVulkan()->GetVulkanInstance(), "vkGetDeviceProcAddr");
        if (Ext_VK_ARM_tensors && Ext_VK_ARM_data_graph && fpGetDeviceProcAddr)
        {
            m_IsGraphPipelinesSupported = true;
        }
    }
#endif

    // NOTE: You should configure these according to what the model expects.
    m_RenderResolution   = glm::ivec2(960, 540);
    m_UpscaledResolution = glm::ivec2(1920, 1080); // It just happens that this aligns with the default values of gSurfaceWidth and gSurfaceHeight.

    if (!InitializeCamera())
    {
        return false;
    }
    
    if (!InitializeLights())
    {
        return false;
    }

    if (!LoadShaders())
    {
        return false;
    }
     
    if (!CreateTensors())
    {
        return false;
    }
    
    if (!CreateGraphPipeline())
    {
        return false;
    }

    if (!InitUniforms())
    {
        return false;
    }

    if (!CreateRenderTargets())
    {
        return false;
    }

    if (!InitAllRenderPasses())
    {
        return false;
    }

    if (!InitGui(windowHandle))
    {
        return false;
    }

    if (!LoadMeshObjects())
    {
        return false;
    }

    if (!InitCommandBuffers())
    {
        return false;
    }

    if (!InitLocalSemaphores())
    {
        return false;
    }

    if (!BuildCmdBuffers())
    {
        return false;
    }

    if (!GetVulkan()->IsDataGraphQueueSupported())
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Destroy()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    // Uniform Buffers
    ReleaseUniformBuffer(pVulkan, &m_ObjectVertUniform);
    ReleaseUniformBuffer(pVulkan, &m_LightUniform);
     
    for (auto& [hash, objectUniform] : m_ObjectFragUniforms)
    {
        ReleaseUniformBuffer(pVulkan, &objectUniform.objectFragUniform);
    }

    // Cmd buffers
    for (int whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        for (auto& cmdBuffer : m_RenderPassData[whichPass].PassCmdBuffer)
        {
            cmdBuffer.Release();
        }

        for (auto& cmdBuffer : m_RenderPassData[whichPass].ObjectsCmdBuffer)
        {
            cmdBuffer.Release();
        }

        m_RenderPassData[whichPass].RenderTarget.Release();
    }

    // Render passes / Semaphores
    for (int whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        vkDestroyRenderPass(pVulkan->m_VulkanDevice, m_RenderPassData[whichPass].RenderPass, nullptr);
        vkDestroySemaphore(pVulkan->m_VulkanDevice, m_RenderPassData[whichPass].PassCompleteSemaphore, nullptr);
    }

    // Drawables
    m_SceneDrawables.clear();
    m_BlitQuadDrawable.reset();

    // Internal
    m_ShaderManager.reset();
    m_MaterialManager.reset();
    m_CameraController.reset();
    m_AssetManager.reset();

    ApplicationHelperBase::Destroy();
}

//-----------------------------------------------------------------------------
bool Application::InitializeLights()
//-----------------------------------------------------------------------------
{
    m_LightUniformData.SpotLights_pos[0] = glm::vec4(-6.900000f, 32.299999f, -1.900000f, 1.0f);
    m_LightUniformData.SpotLights_pos[1] = glm::vec4(3.300000f, 26.900000f, 7.600000f, 1.0f);
    m_LightUniformData.SpotLights_pos[2] = glm::vec4(12.100000f, 41.400002f, -2.800000f, 1.0f);
    m_LightUniformData.SpotLights_pos[3] = glm::vec4(-5.400000f, 18.500000f, 28.500000f, 1.0f);

    m_LightUniformData.SpotLights_dir[0] = glm::vec4(-0.534696f, -0.834525f, 0.132924f, 0.0f);
    m_LightUniformData.SpotLights_dir[1] = glm::vec4(0.000692f, -0.197335f, 0.980336f, 0.0f);
    m_LightUniformData.SpotLights_dir[2] = glm::vec4(0.985090f, -0.172016f, 0.003000f, 0.0f);
    m_LightUniformData.SpotLights_dir[3] = glm::vec4(0.674125f, -0.295055f, -0.677125f, 0.0f);

    m_LightUniformData.SpotLights_color[0] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 3.000000f);
    m_LightUniformData.SpotLights_color[1] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 3.500000f);
    m_LightUniformData.SpotLights_color[2] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 2.000000f);
    m_LightUniformData.SpotLights_color[3] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 2.800000f);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitializeCamera()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Initializing Camera...");
    LOGI("******************************");

    m_Camera.SetPosition(gCameraStartPos, glm::quat(gCameraStartRot * TO_RADIANS));
    m_Camera.SetAspect(float(gRenderWidth) / float(gRenderHeight));
    m_Camera.SetFov(gFOV);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    // Camera Controller //

#if defined(OS_ANDROID)
    typedef CameraControllerTouch           tCameraController;
#else
    typedef CameraController                tCameraController;
#endif

    auto cameraController = std::make_unique<tCameraController>();
    if (!cameraController->Initialize(gRenderWidth, gRenderHeight))
    {
        return false;
    }

    m_CameraController = std::move(cameraController);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::CreateTensors()
//-----------------------------------------------------------------------------
{
    if (!m_IsGraphPipelinesSupported)
    {
        LOGI("Not creating Tensors as base extension isn't supported");
        return true;
    }

    auto& vulkan = *GetVulkan();
    const auto& tensor_extension = vulkan.GetExtension<ExtensionHelper::Ext_VK_ARM_tensors>();
    assert(tensor_extension != nullptr);

    LOGI("Creating Tensors...");

    const int64_t componentsPerPixel = 3; // R8G8B8_UNORM and Model is RGB

    m_InputTensor.strides          = { componentsPerPixel * m_RenderResolution.x, componentsPerPixel, 1 };
    m_InputTensor.dimensions       = { m_RenderResolution.y, m_RenderResolution.x, componentsPerPixel };
    m_InputTensor.portBindingIndex = m_QNNInputPortBinding;

    m_OutputTensor.strides          = { componentsPerPixel * m_UpscaledResolution.x, componentsPerPixel, 1 };
    m_OutputTensor.dimensions       = { m_UpscaledResolution.y, m_UpscaledResolution.x, componentsPerPixel };
    m_OutputTensor.portBindingIndex = m_QNNOutputPortBinding;

    auto CreateTensor = [&](GraphPipelineTensor& targetTensor) -> bool
    {
        const uint32_t bufferSize = targetTensor.dimensions[0] * targetTensor.dimensions[1] * targetTensor.dimensions[2];

        // TENSOR OBJECT //

        LOGI("Creating Tensors Object");

        targetTensor.tensorDescription = VkTensorDescriptionARM
        {
            .sType = VK_STRUCTURE_TYPE_TENSOR_DESCRIPTION_ARM,
            .pNext = nullptr,
            .tiling = VK_TENSOR_TILING_LINEAR_ARM, // VK_TENSOR_TILING_OPTIMAL_ARM TODO: Find out why it cannot be optimal
            .format = VK_FORMAT_R8_UNORM,
            .dimensionCount = static_cast<uint32_t>(targetTensor.dimensions.size()),
            .pDimensions = targetTensor.dimensions.data(),
            .pStrides = nullptr,
            .usage = VK_TENSOR_USAGE_DATA_GRAPH_BIT_ARM/* | VK_TENSOR_USAGE_SHADER_BIT_ARM*/
        };

        VkExternalMemoryTensorCreateInfoARM externalInfo
        {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_TENSOR_CREATE_INFO_ARM,
            .pNext = nullptr,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
        };

        VkTensorCreateInfoARM tensorInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_TENSOR_CREATE_INFO_ARM,
            .pNext = &externalInfo,
            .flags = 0,
            .pDescription = &targetTensor.tensorDescription,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        if (tensor_extension->m_vkCreateTensorARM(vulkan.m_VulkanDevice, &tensorInfo, nullptr, &targetTensor.tensor) != VK_SUCCESS)
        {
            return false;
        }

        // TENSOR MEMORY REQUIREMENTS //

#if 0
        VkMemoryDedicatedAllocateInfoTensorARM dedicatedInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_TENSOR_ARM,
            .pNext = nullptr,
            .tensor = targetTensor.tensor
        };
#endif

#if 0
        VkTensorMemoryRequirementsInfoARM memReqInfo =
        {
            .sType = VK_STRUCTURE_TYPE_TENSOR_MEMORY_REQUIREMENTS_INFO_ARM,
            .pNext = nullptr,
            .tensor = targetTensor.tensor
        };

        VkMemoryRequirements2 memReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        tensor_extension->m_vkGetTensorMemoryRequirementsARM(vulkan.m_VulkanDevice, &memReqInfo, &memReq);
#else

        VkDeviceTensorMemoryRequirementsARM deviceMemReqInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_TENSOR_MEMORY_REQUIREMENTS_ARM,
            .pNext = nullptr,
            .pCreateInfo = &tensorInfo
        };
        VkMemoryRequirements2 memReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        tensor_extension->m_vkGetDeviceTensorMemoryRequirementsARM(vulkan.m_VulkanDevice, &deviceMemReqInfo, &memReq);
#endif

        // TENSOR ALIASED BUFFER //

        LOGI("Creating Tensor Aliased Buffer - Tensor Size: %d - Buffer Size: %d", memReq.memoryRequirements.size, bufferSize);

        // Create buffer with aliasing usage
        VkBufferCreateInfo bufferInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = bufferSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT/* | VK_BUFFER_USAGE_2_DATA_GRAPH_FOREIGN_DESCRIPTOR_BIT_ARM*/,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        if (vkCreateBuffer(vulkan.m_VulkanDevice, &bufferInfo, nullptr, &targetTensor.aliasedBuffer) != VK_SUCCESS)
        {
            return false;
        }

        // TENSOR MEMORY //

        LOGI("Allocating Tensor Memory");

        VkExportMemoryAllocateInfo exportAllocInfo =
        {
            .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
        };

        VkMemoryRequirements bufferMemReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        vkGetBufferMemoryRequirements(vulkan.m_VulkanDevice, targetTensor.aliasedBuffer, &bufferMemReq);

        VkMemoryAllocateInfo allocInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &exportAllocInfo,
            .allocationSize = bufferMemReq.size,
            .memoryTypeIndex = FindMemoryType(vulkan.m_VulkanGpu, bufferMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        if (vkAllocateMemory(vulkan.m_VulkanDevice, &allocInfo, nullptr, &targetTensor.tensorMemory) != VK_SUCCESS)
        {
            return false;
        }

        VkBindTensorMemoryInfoARM bindInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_BIND_TENSOR_MEMORY_INFO_ARM,
            .pNext = nullptr,
            .tensor = targetTensor.tensor,
            .memory = targetTensor.tensorMemory,
            .memoryOffset = 0
        };

        LOGI("Binding Tensor Memory");

        if(tensor_extension->m_vkBindTensorMemoryARM(vulkan.m_VulkanDevice, 1, &bindInfo) != VK_SUCCESS)
        {
            return false;
        }

        LOGI("Binding Aliased Buffer Memory");

        if (vkBindBufferMemory(vulkan.m_VulkanDevice, targetTensor.aliasedBuffer, targetTensor.tensorMemory, 0) != VK_SUCCESS)
        {
            return false;
        }

        // TENSOR VIEW //

        LOGI("Creating Tensor View");

        VkTensorViewCreateInfoARM viewInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_TENSOR_VIEW_CREATE_INFO_ARM,
            .pNext = nullptr,
            .flags = 0,
            .tensor = targetTensor.tensor,
            .format = targetTensor.tensorDescription.format
        };

        if (tensor_extension->m_vkCreateTensorViewARM(vulkan.m_VulkanDevice, &viewInfo, nullptr, &targetTensor.tensorView) != VK_SUCCESS)
        {
            return false;
        }

        return true;
    };

    if (!CreateTensor(m_InputTensor))
    {
        return false;
    }

    if (!CreateTensor(m_OutputTensor))
    {
        return false;
    }

    LOGI("Creating Tensors Descriptor Pool");

    VkDataGraphProcessingEngineCreateInfoARM engineInfo = {};
    VkDescriptorPoolCreateInfo               descPoolInfo = {};

    engineInfo.sType = VK_STRUCTURE_TYPE_DATA_GRAPH_PROCESSING_ENGINE_CREATE_INFO_ARM;
    engineInfo.processingEngineCount = 1;
    engineInfo.pProcessingEngines = &vulkan.m_VulkanDataGraphProcessingEngine;

    VkDescriptorPoolSize pool = {};

    pool.type = VK_DESCRIPTOR_TYPE_TENSOR_ARM;
    pool.descriptorCount = m_QNNMaxPortIndex + 1;

    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.pNext = &engineInfo;
    descPoolInfo.maxSets = 1;
    descPoolInfo.poolSizeCount = 1;
    descPoolInfo.pPoolSizes = &pool;
    if (vkCreateDescriptorPool(vulkan.m_VulkanDevice, &descPoolInfo, NULL, &m_TensorDescriptorPool) != VK_SUCCESS)
    {
        return false;
    }

    // TENSOR DESCRIPTOR SET LAYOUT //

    LOGI("Creating Tensor Descriptor Set Layout");

    VkDescriptorSetLayoutCreateInfo descLayoutInfo = {};
    std::array< VkDescriptorSetLayoutBinding, 2> bindings = {};

    bindings[0].binding = m_InputTensor.portBindingIndex;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_TENSOR_ARM;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

    bindings[1].binding = m_OutputTensor.portBindingIndex;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_TENSOR_ARM;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

    descLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutInfo.pNext        = NULL;
    descLayoutInfo.flags        = 0;
    descLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    descLayoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(vulkan.m_VulkanDevice, &descLayoutInfo, NULL, &m_TensorDescriptorSetLayout) != VK_SUCCESS)
    {
        return false;
    }

    // TENSOR DESCRIPTOR SETS //

    LOGI("Creating Tensor Descriptor Sets");

    // Allocate Descriptor Sets:
    VkDescriptorSetAllocateInfo info = {};

    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool     = m_TensorDescriptorPool;
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &m_TensorDescriptorSetLayout;

    if (vkAllocateDescriptorSets(vulkan.m_VulkanDevice, &info, &m_TensorDescriptorSet) != VK_SUCCESS)
    {
        return false;
    }

    // UPDATE/BIND TENSOR DESCRIPTOR SETS //

    LOGI("Updating Tensor Descriptor Sets");

    //Bind tensors to descriptor set
    VkWriteDescriptorSet           write[2];
    VkWriteDescriptorSetTensorARM  tensorWrite[2];

    tensorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM;
    tensorWrite[0].pNext = NULL;
    tensorWrite[0].tensorViewCount = 1;
    tensorWrite[0].pTensorViews = &m_InputTensor.tensorView;

    tensorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM;
    tensorWrite[1].pNext = NULL;
    tensorWrite[1].tensorViewCount = 1;
    tensorWrite[1].pTensorViews = &m_OutputTensor.tensorView;

    write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write[0].pNext = &tensorWrite[0];
    write[0].dstBinding = m_InputTensor.portBindingIndex;
    write[0].descriptorCount = 1;
    write[0].descriptorType = VK_DESCRIPTOR_TYPE_TENSOR_ARM;
    write[0].dstSet = m_TensorDescriptorSet;
    write[0].dstArrayElement = 0;
    write[0].pBufferInfo = NULL;
    write[0].pImageInfo = NULL;
    write[0].pTexelBufferView = NULL;

    write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write[1].pNext = &tensorWrite[1];
    write[1].dstBinding = m_OutputTensor.portBindingIndex;
    write[1].descriptorCount = 1;
    write[1].descriptorType = VK_DESCRIPTOR_TYPE_TENSOR_ARM;
    write[1].dstSet = m_TensorDescriptorSet;
    write[1].dstArrayElement = 0;
    write[1].pBufferInfo = NULL;
    write[1].pImageInfo = NULL;
    write[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(vulkan.m_VulkanDevice, 2, write, 0, NULL);

    LOGI("Tensors Objects Created!");

    return true;
}

//-----------------------------------------------------------------------------
bool Application::CreateGraphPipeline()
//-----------------------------------------------------------------------------
{
    if (!m_IsGraphPipelinesSupported)
    {
        LOGI("Not creating Graph Pipeline as base extension isn't supported");
        return true;
    }

    auto& vulkan = *GetVulkan();
    const auto& data_graph_extension = vulkan.GetExtension<ExtensionHelper::Ext_VK_ARM_data_graph>();
    assert(data_graph_extension != nullptr);
    
    LOGI("Loading file model from disk...");

    std::vector<unsigned char> modelData;
    {
        if (!m_AssetManager->LoadFileIntoMemory(gModelAssetPath, modelData))
        {
            LOGE("Failed to load Model file, disabling the Graph Pipelines extension");
            m_IsGraphPipelinesSupported = false;
            return true;
        }
    }

    LOGI("Creating Pipeline Cache from Model...");

    VkPipelineCacheCreateInfo cacheInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .initialDataSize = modelData.size(),
        .pInitialData = modelData.data()
    };

    if (vkCreatePipelineCache(vulkan.m_VulkanDevice, &cacheInfo, nullptr, &m_GraphPipelineInstance.pipelineCache) != VK_SUCCESS)
    {
        return false;
    }

    LOGI("Creating Graph Pipeline Layout...");

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &m_TensorDescriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    if (vkCreatePipelineLayout(vulkan.m_VulkanDevice, &pipelineLayoutInfo, nullptr, &m_GraphPipelineInstance.pipelineLayout) != VK_SUCCESS)
    {
        return false;
    }

    LOGI("Creating Graph Pipeline...");

    VkDataGraphPipelineResourceInfoARM resourceInfos[2];

    resourceInfos[0].sType = VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_RESOURCE_INFO_ARM;
    resourceInfos[0].binding = m_QNNInputPortBinding; // Same as the input tensor
    resourceInfos[0].pNext = &m_InputTensor.tensorDescription;

    resourceInfos[1].sType = VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_RESOURCE_INFO_ARM;
    resourceInfos[1].binding = m_QNNOutputPortBinding; // Same as the output tensor
    resourceInfos[1].pNext = &m_OutputTensor.tensorDescription;

    ////////////////////
    // IMPORTANT NOTE // These values should be read from the file identifier!!!
    ////////////////////

    uint32_t graphId = 0;
    uint32_t qnnGraphIdSize = sizeof(uint32_t);
    uint8_t  qnnGraphId[32];
    std::memcpy(qnnGraphId, &graphId, qnnGraphIdSize);

    ////////////////////
    ////////////////////
    ////////////////////
    
    VkDataGraphProcessingEngineCreateInfoARM engineInfo = { VK_STRUCTURE_TYPE_DATA_GRAPH_PROCESSING_ENGINE_CREATE_INFO_ARM };
    engineInfo.processingEngineCount = 1;
    engineInfo.pProcessingEngines = &vulkan.m_VulkanDataGraphProcessingEngine;

    VkDataGraphPipelineIdentifierCreateInfoARM identifier = { VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_IDENTIFIER_CREATE_INFO_ARM };
    identifier.pNext = &engineInfo;
    identifier.identifierSize = qnnGraphIdSize;
    identifier.pIdentifier = qnnGraphId;

    VkDataGraphPipelineShaderModuleCreateInfoARM moduleInfo = { VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SHADER_MODULE_CREATE_INFO_ARM };
    moduleInfo.pNext = &identifier;
    moduleInfo.module = VK_NULL_HANDLE;
    moduleInfo.pName = "";

    VkDataGraphPipelineCreateInfoARM pipelineInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_CREATE_INFO_ARM,
        .pNext = &moduleInfo,
        .flags = 0,
        .layout = m_GraphPipelineInstance.pipelineLayout,
        .resourceInfoCount = 2,
        .pResourceInfos = resourceInfos
    };

    if (data_graph_extension->m_vkCreateDataGraphPipelinesARM(
        vulkan.m_VulkanDevice, 
        VK_NULL_HANDLE, 
        m_GraphPipelineInstance.pipelineCache, 
        1, 
        &pipelineInfo, 
        nullptr, 
        &m_GraphPipelineInstance.graphPipeline) != VK_SUCCESS)
    {
        return false;
    }

    LOGI("Creating Graph Pipeline Session...");

    VkDataGraphPipelineSessionCreateInfoARM sessionInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_CREATE_INFO_ARM,
        .pNext = nullptr,
        .flags = 0,
        .dataGraphPipeline = m_GraphPipelineInstance.graphPipeline
    };

    if (data_graph_extension->m_vkCreateDataGraphPipelineSessionARM(
        vulkan.m_VulkanDevice, 
        &sessionInfo, 
        nullptr, 
        &m_GraphPipelineInstance.graphSession) != VK_SUCCESS)
    {
        return false;
    }

    LOGI("Getting Graph Session Binding Points Requirements...");

    VkDataGraphPipelineSessionBindPointRequirementsInfoARM bindReqsInfo = {};
    uint32_t                                               bindReqsCount = 0;

    bindReqsInfo.sType = VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_REQUIREMENTS_INFO_ARM;
    bindReqsInfo.session = m_GraphPipelineInstance.graphSession;

    if (data_graph_extension->m_vkGetDataGraphPipelineSessionBindPointRequirementsARM(vulkan.m_VulkanDevice, &bindReqsInfo, &bindReqsCount, NULL) != VK_SUCCESS)
    {
        return false;
    }

    std::vector<VkDataGraphPipelineSessionBindPointRequirementARM> bindReqs(bindReqsCount);
    for (uint32_t i = 0; i < bindReqsCount; i++)
    {
        bindReqs[i].sType = VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_REQUIREMENT_ARM;
    }

    if (data_graph_extension->m_vkGetDataGraphPipelineSessionBindPointRequirementsARM(vulkan.m_VulkanDevice, &bindReqsInfo, &bindReqsCount, bindReqs.data()) != VK_SUCCESS)
    {
        return false;
    }

    LOGI("Binding Graph Session Memory...");

    uint32_t memCount = 0;
    for (uint32_t i = 0; i < bindReqsCount; i++)
    {
        m_GraphPipelineInstance.sessionMemory.resize(m_GraphPipelineInstance.sessionMemory.size() + bindReqs[i].numObjects);
        switch (bindReqs[i].bindPointType) 
        {
            case(VK_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_TYPE_MEMORY_ARM):
            {
                LOGI("*** Bind Point (VK_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_TYPE_MEMORY_ARM) with %d objects", bindReqs[i].numObjects);
                for (uint32_t j = 0; j < bindReqs[i].numObjects; j++)
                {
                    VkDataGraphPipelineSessionMemoryRequirementsInfoARM memReqsInfo = { VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_MEMORY_REQUIREMENTS_INFO_ARM };
                    memReqsInfo.session     = m_GraphPipelineInstance.graphSession;
                    memReqsInfo.bindPoint   = bindReqs[i].bindPoint;
                    memReqsInfo.objectIndex = j;

                    VkMemoryRequirements2 memReqs = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
                    data_graph_extension->m_vkGetDataGraphPipelineSessionMemoryRequirementsARM(vulkan.m_VulkanDevice, &memReqsInfo, &memReqs);

                    VkMemoryAllocateInfo info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
                    info.pNext = nullptr;
                    info.allocationSize = memReqs.memoryRequirements.size;
                    // info.memoryTypeIndex = 0; // should query the indices to find most appropiate one
                    info.memoryTypeIndex = FindMemoryType(vulkan.m_VulkanGpu, memReqs.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

                    LOGI("***     Bind Object [%d]", j);
                    LOGI("***         Binding Point [%d]", bindReqs[i].bindPoint);
                    LOGI("***         Allocation Size [%d]", info.allocationSize);
                    LOGI("***         Memory Type Index [%d]", info.memoryTypeIndex);

                    if (vkAllocateMemory(vulkan.m_VulkanDevice, &info, nullptr, &m_GraphPipelineInstance.sessionMemory[memCount]) != VK_SUCCESS)
                    {
                        return false;
                    }

                    VkBindDataGraphPipelineSessionMemoryInfoARM bindMem;
                    bindMem.sType       = VK_STRUCTURE_TYPE_BIND_DATA_GRAPH_PIPELINE_SESSION_MEMORY_INFO_ARM;
                    bindMem.session     = m_GraphPipelineInstance.graphSession;
                    bindMem.bindPoint   = bindReqs[i].bindPoint;
                    bindMem.objectIndex = j;
                    bindMem.memory      = m_GraphPipelineInstance.sessionMemory[memCount];

                    if (data_graph_extension->m_vkBindDataGraphPipelineSessionMemoryARM(vulkan.m_VulkanDevice, 1, &bindMem) != VK_SUCCESS)
                    {
                        return false;
                    }

                    memCount++;
                }

                break;
            }
            default:
            {
                // Error unhandled / unexpected memory type
                return false;
            }
        }
    } 

    LOGI("Graph Pipeline Created!");

    return true;
}

//-----------------------------------------------------------------------------
void Application::CopyImageToTensor(
    CommandListVulkan&         cmdList,
    TextureVulkan&             srcImage,
    VkImageLayout              currentLayout, 
    const GraphPipelineTensor& tensorBinding)
//-----------------------------------------------------------------------------
{
    const auto& synchronization2_extension = GetVulkan()->GetExtension<ExtensionHelper::Ext_VK_KHR_synchronization2>();
    assert(synchronization2_extension != nullptr);

    VkImageMemoryBarrier2 imageBarrierToTransfer = 
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .oldLayout = currentLayout,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = srcImage.GetVkImage(),
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo depInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrierToTransfer
    };

    synchronization2_extension->m_vkCmdPipelineBarrier2KHR(cmdList.m_VkCommandBuffer, &depInfo);

    VkBufferImageCopy copyRegion = 
    {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {srcImage.Width, srcImage.Height, 1}
    };
    
    vkCmdCopyImageToBuffer(
        cmdList.m_VkCommandBuffer, 
        srcImage.GetVkImage(), 
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        tensorBinding.aliasedBuffer, 
        1, 
        &copyRegion);

    // Transition image back to original layout
    std::swap(imageBarrierToTransfer.oldLayout, imageBarrierToTransfer.newLayout);
    std::swap(imageBarrierToTransfer.srcAccessMask, imageBarrierToTransfer.dstAccessMask);
    std::swap(imageBarrierToTransfer.srcStageMask, imageBarrierToTransfer.dstStageMask);

    synchronization2_extension->m_vkCmdPipelineBarrier2KHR(cmdList.m_VkCommandBuffer, &depInfo);
}

//-----------------------------------------------------------------------------
void Application::CopyTensorToImage(
    CommandListVulkan&         cmdList,
    TextureVulkan&             dstImage,
    VkImageLayout              currentLayout, 
    const GraphPipelineTensor& tensorBinding)
//-----------------------------------------------------------------------------
{
    const auto& synchronization2_extension = GetVulkan()->GetExtension<ExtensionHelper::Ext_VK_KHR_synchronization2>();
    assert(synchronization2_extension != nullptr);

    VkImageMemoryBarrier2 imageBarrierToTransfer = 
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = currentLayout,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = dstImage.GetVkImage(),
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo depInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrierToTransfer
    };

    synchronization2_extension->m_vkCmdPipelineBarrier2KHR(cmdList.m_VkCommandBuffer, &depInfo);

    VkBufferImageCopy copyRegion = 
    {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {dstImage.Width, dstImage.Height, 1}
    };

    vkCmdCopyBufferToImage(
        cmdList.m_VkCommandBuffer, 
        tensorBinding.aliasedBuffer, 
        dstImage.GetVkImage(), 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, 
        &copyRegion);

    std::swap(imageBarrierToTransfer.oldLayout, imageBarrierToTransfer.newLayout);
    std::swap(imageBarrierToTransfer.srcAccessMask, imageBarrierToTransfer.dstAccessMask);
    std::swap(imageBarrierToTransfer.srcStageMask, imageBarrierToTransfer.dstStageMask);

    synchronization2_extension->m_vkCmdPipelineBarrier2KHR(cmdList.m_VkCommandBuffer, &depInfo);
}


//-----------------------------------------------------------------------------
void Application::CopyImageToImageBlit(
    CommandListVulkan& cmdList,
    TextureVulkan&     srcImage,
    VkImageLayout      srcLayout,
    TextureVulkan&     dstImage,
    VkImageLayout      dstFinalLayout)
//-----------------------------------------------------------------------------
{
    const auto& synchronization2_extension = GetVulkan()->GetExtension<ExtensionHelper::Ext_VK_KHR_synchronization2>();
    assert(synchronization2_extension != nullptr);

    VkImageMemoryBarrier2 dstBarrier = 
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = dstImage.GetVkImage(),
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo depInfoDst = 
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &dstBarrier
    };

    synchronization2_extension->m_vkCmdPipelineBarrier2KHR(cmdList.m_VkCommandBuffer, &depInfoDst);

    // Blit image
    VkImageBlit blitRegion = {};
    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcOffsets[0] = { 0, 0, 0 };
    blitRegion.srcOffsets[1] = { static_cast<int32_t>(srcImage.Width), static_cast<int32_t>(srcImage.Height), 1 };

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.mipLevel = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstOffsets[0] = { 0, 0, 0 };
    blitRegion.dstOffsets[1] = { static_cast<int32_t>(dstImage.Width), static_cast<int32_t>(dstImage.Height), 1 };

    vkCmdBlitImage(
        cmdList.m_VkCommandBuffer,
        srcImage.GetVkImage(),
        srcLayout,
        dstImage.GetVkImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &blitRegion,
        VK_FILTER_LINEAR
    );

    // Transition destination image to final layout
    dstBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstBarrier.newLayout = dstFinalLayout;
    dstBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    dstBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    dstBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    dstBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    synchronization2_extension->m_vkCmdPipelineBarrier2KHR(cmdList.m_VkCommandBuffer, &depInfoDst);
}


//-----------------------------------------------------------------------------
bool Application::LoadShaders()
//-----------------------------------------------------------------------------
{
    m_ShaderManager = std::make_unique<ShaderManagerT<Vulkan>>(*GetVulkan());
    m_ShaderManager->RegisterRenderPassNames(sRenderPassNames);

    m_MaterialManager = std::make_unique<MaterialManagerT<Vulkan>>();

    LOGI("******************************");
    LOGI("Loading Shaders...");
    LOGI("******************************");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
            { tIdAndFilename { "Blit",  "Media\\Shaders\\Blit.json" },
              tIdAndFilename { "SceneOpaque", "Media\\Shaders\\SceneOpaque.json" },
              tIdAndFilename { "SceneTransparent", "Media\\Shaders\\SceneTransparent.json" }
            })
    {
        if (!m_ShaderManager->AddShader(*m_AssetManager, i.first, i.second))
        {
            LOGE("Error Loading shader %s from %s", i.first.c_str(), i.second.c_str());
            LOGI("Please verify if you have all required assets on the sample media folder");
            LOGI("If you are running on Android, don't forget to run the `02_CopyMediaToDevice.bat` script to copy all media files into the device memory");
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::CreateRenderTargets()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    LOGI("**************************");
    LOGI("Creating Render Targets...");
    LOGI("**************************");

    TextureFormat vkDesiredDepthFormat = pVulkan->GetBestSurfaceDepthFormat();
    TextureFormat desiredDepthFormat = vkDesiredDepthFormat;

    const TextureFormat MainColorType[]       = { TextureFormat::R8G8B8_UNORM };
    const TEXTURE_TYPE  MainTextureType[]     = { TEXTURE_TYPE::TT_RENDER_TARGET_TRANSFERSRC }; // Needed for tensor copy from operation.
    const TEXTURE_TYPE  UpscaledTextureType   = TEXTURE_TYPE::TT_CPU_UPDATE;                    // Needed for tensor copy to operation.
    const TextureFormat HudColorType[]        = { TextureFormat::R8G8B8A8_SRGB };

    if (!m_RenderPassData[RP_SCENE].RenderTarget.Initialize(
        pVulkan, 
        m_RenderResolution.x, 
        m_RenderResolution.y, 
        MainColorType, 
        desiredDepthFormat, 
        VK_SAMPLE_COUNT_1_BIT, 
        "Scene RT", 
        MainTextureType))
    {
        LOGE("Unable to create scene render target");
        return false;
    }
    
    {
        CreateTexObjectInfo createInfo{};
        createInfo.uiWidth = m_UpscaledResolution.x;
        createInfo.uiHeight = m_UpscaledResolution.y;

        createInfo.Format = MainColorType[0];
        createInfo.TexType = UpscaledTextureType;
        createInfo.pName = "Upscaled RT";
        createInfo.Msaa = VK_SAMPLE_COUNT_1_BIT;
        createInfo.FilterMode = SamplerFilter::Linear;

        m_UpscaledImageResult = CreateTextureObject(*GetVulkan(), createInfo);
    }

    // Notice no depth on the HUD RT
    if (!m_RenderPassData[RP_HUD].RenderTarget.Initialize(pVulkan, gSurfaceWidth, gSurfaceHeight, HudColorType, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "HUD RT"))
    {
        LOGE("Unable to create hud render target");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitUniforms()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Initializing Uniforms...");
    LOGI("******************************");

    Vulkan* const pVulkan = GetVulkan();

    if (!CreateUniformBuffer(pVulkan, m_ObjectVertUniform))
    {
        return false;
    }

    if (!CreateUniformBuffer(pVulkan, m_LightUniform))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitAllRenderPasses()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    //                                       ColorInputUsage |               ClearDepthRenderPass | ColorOutputUsage |                     DepthOutputUsage |              ClearColor
    m_RenderPassData[RP_SCENE].PassSetup = { RenderPassInputUsage::Clear,    true,                  RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Store,   {}};
    m_RenderPassData[RP_HUD].PassSetup   = { RenderPassInputUsage::Clear,    false,                 RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Discard, {}};
    m_RenderPassData[RP_BLIT].PassSetup  = { RenderPassInputUsage::DontCare, true,                  RenderPassOutputUsage::Present,        RenderPassOutputUsage::Discard, {}};

    TextureFormat surfaceFormat = pVulkan->m_SurfaceFormat;
    auto swapChainColorFormat = std::span<const TextureFormat>({ &surfaceFormat, 1 });
    auto swapChainDepthFormat = pVulkan->m_SwapchainDepth.format;

    LOGI("******************************");
    LOGI("Initializing Render Passes... ");
    LOGI("******************************");

    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        bool isSwapChainRenderPass = whichPass == RP_BLIT;

        std::span<const TextureFormat> colorFormats = isSwapChainRenderPass ? swapChainColorFormat : m_RenderPassData[whichPass].RenderTarget[0].m_pLayerFormats;
        TextureFormat                  depthFormat  = isSwapChainRenderPass ? swapChainDepthFormat : m_RenderPassData[whichPass].RenderTarget[0].m_DepthFormat;

        const auto& passSetup = m_RenderPassData[whichPass].PassSetup;
        
        if (!pVulkan->CreateRenderPass(
            { colorFormats },
            depthFormat,
            VK_SAMPLE_COUNT_1_BIT,
            passSetup.ColorInputUsage,
            passSetup.ColorOutputUsage,
            passSetup.ClearDepthRenderPass,
            passSetup.DepthOutputUsage,
            & m_RenderPassData[whichPass].RenderPass))
        {
            return false;
        }
            
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    const auto& hudRenderTarget = m_RenderPassData[RP_HUD].RenderTarget;
    m_Gui = std::make_unique<GuiImguiGfx<Vulkan>>(*GetVulkan(), m_RenderPassData[RP_HUD].RenderPass);
    if (!m_Gui->Initialize(windowHandle, hudRenderTarget[0].m_Width, hudRenderTarget[0].m_Height))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadMeshObjects()
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    LOGI("***********************");
    LOGI("Initializing Meshes... ");
    LOGI("***********************");

    const auto* pSceneOpaqueShader      = m_ShaderManager->GetShader("SceneOpaque");
    const auto* pSceneTransparentShader = m_ShaderManager->GetShader("SceneTransparent");
    const auto* pBlitQuadShader         = m_ShaderManager->GetShader("Blit");
    if (!pSceneOpaqueShader || !pSceneTransparentShader || !pBlitQuadShader)
    {
        return false;
    }
    
    LOGI("***********************************");
    LOGI("Loading and preparing the museum...");
    LOGI("***********************************");

    m_TextureManager->SetDefaultFilenameManipulators(PathManipulator_PrefixDirectory{ "Media\\" }, PathManipulator_ChangeExtension{ ".ktx" });

    auto* whiteTexture         = m_TextureManager->GetOrLoadTexture(*m_AssetManager, "Textures\\white_d.ktx", m_SamplerEdgeClamp);
    auto* blackTexture         = m_TextureManager->GetOrLoadTexture(*m_AssetManager, "Textures\\black_d.ktx", m_SamplerEdgeClamp);
    auto* normalDefaultTexture = m_TextureManager->GetOrLoadTexture(*m_AssetManager, "Textures\\normal_default.ktx", m_SamplerEdgeClamp);

    if (!whiteTexture || !blackTexture || !normalDefaultTexture)
    {
        LOGE("Failed to load supporting textures");
        return false;
    }

    auto UniformBufferLoader = [&](const ObjectMaterialParameters& objectMaterialParameters) -> ObjectMaterialParameters&
    {
        auto hash = objectMaterialParameters.GetHash();

        auto iter = m_ObjectFragUniforms.try_emplace(hash, ObjectMaterialParameters());
        if (iter.second)
        {
            iter.first->second.objectFragUniformData = objectMaterialParameters.objectFragUniformData;
            if (!CreateUniformBuffer(pVulkan, iter.first->second.objectFragUniform))
            {
                LOGE("Failed to create object uniform buffer");
            }
        }

        return iter.first->second;
    };

    auto MaterialLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef)->std::optional<Material>
    {
        auto* diffuseTexture           = m_TextureManager->GetOrLoadTexture(*m_AssetManager, materialDef.diffuseFilename, m_SamplerEdgeClamp);
        auto* normalTexture            = m_TextureManager->GetOrLoadTexture(*m_AssetManager, materialDef.bumpFilename, m_SamplerEdgeClamp);
        auto* emissiveTexture          = m_TextureManager->GetOrLoadTexture(*m_AssetManager, materialDef.emissiveFilename, m_SamplerEdgeClamp);
        auto* metallicRoughnessTexture = m_TextureManager->GetOrLoadTexture(*m_AssetManager, materialDef.specMapFilename, m_SamplerEdgeClamp);
        bool alphaCutout               = materialDef.alphaCutout;
        bool transparent               = materialDef.transparent;

        const Shader* targetShader = transparent ? pSceneTransparentShader : pSceneOpaqueShader;

        ObjectMaterialParameters objectMaterial;
        objectMaterial.objectFragUniformData.Color.r = static_cast<float>(materialDef.baseColorFactor[0]);
        objectMaterial.objectFragUniformData.Color.g = static_cast<float>(materialDef.baseColorFactor[1]);
        objectMaterial.objectFragUniformData.Color.b = static_cast<float>(materialDef.baseColorFactor[2]);
        objectMaterial.objectFragUniformData.Color.a = static_cast<float>(materialDef.baseColorFactor[3]);
        objectMaterial.objectFragUniformData.ORM.b   = static_cast<float>(materialDef.metallicFactor);
        objectMaterial.objectFragUniformData.ORM.g   = static_cast<float>(materialDef.roughnessFactor);

        if (diffuseTexture == nullptr || normalTexture == nullptr)
        {
            return std::nullopt;
        }

        auto shaderMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *targetShader, NUM_VULKAN_BUFFERS,
            [&](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
            {
                if (texName == "Diffuse")
                {
                    return { diffuseTexture ? diffuseTexture : whiteTexture };
                }
                if (texName == "Normal")
                {
                    return { normalTexture ? normalTexture : normalDefaultTexture };
                }
                if (texName == "Emissive")
                {
                    return { emissiveTexture ? emissiveTexture : blackTexture };
                }
                if (texName == "MetallicRoughness")
                {
                    return { metallicRoughnessTexture ? metallicRoughnessTexture : blackTexture };
                }

                return {};
            },
            [&](const std::string& bufferName) -> tPerFrameVkBuffer
            {
                if (bufferName == "Vert")
                {
                    return { m_ObjectVertUniform.buf.GetVkBuffer() };
                }
                else if (bufferName == "Frag")
                {
                    return { UniformBufferLoader(objectMaterial).objectFragUniform.buf.GetVkBuffer() };
                }
                else if (bufferName == "Light")
                {
                    return { m_LightUniform.buf.GetVkBuffer() };
                }

                return {};
            }
            );

        return shaderMaterial;
    };


    const auto loaderFlags = 0; // No instancing
    const bool ignoreTransforms = (loaderFlags & DrawableLoader::LoaderFlags::IgnoreHierarchy) != 0;

    MeshLoaderModelSceneSanityCheck meshSanityCheckProcessor(gMuseumAssetPath);
    MeshObjectIntermediateGltfProcessor meshObjectProcessor(gMuseumAssetPath, ignoreTransforms, glm::vec3(1.0f,1.0f,1.0f));
    CameraGltfProcessor meshCameraProcessor{};

    if (!MeshLoader::LoadGltf(*m_AssetManager, gMuseumAssetPath, meshSanityCheckProcessor, meshObjectProcessor, meshCameraProcessor) ||
        !DrawableLoader::CreateDrawables(*pVulkan,
                                        std::move(meshObjectProcessor.m_meshObjects),
                                        { &m_RenderPassData[RP_SCENE].RenderPass, 1 },
                                        &sRenderPassNames[RP_SCENE],
                                        MaterialLoader,
                                        m_SceneDrawables,
                                        {},    // RenderPassMultisample
                                        loaderFlags,
                                        {}))   // RenderPassSubpasses
    {
        LOGE("Error Loading the museum gltf file");
        LOGI("Please verify if you have all required assets on the sample media folder");
        LOGI("If you are running on Android, don't forget to run the `02_CopyMediaToDevice.bat` script to copy all media files into the device memory");
        return false;
    }

    if (!meshCameraProcessor.m_cameras.empty())
    {
        const auto& camera = meshCameraProcessor.m_cameras[0];
        m_Camera.SetPosition(camera.Position, camera.Orientation);
    }


    LOGI("*********************");
    LOGI("Creating Quad mesh...");
    LOGI("*********************");

    MeshObject blitQuadMesh;
    MeshHelper::CreateScreenSpaceMesh(pVulkan->GetMemoryManager(), 0, &blitQuadMesh);

    // Blit Material
    auto blitQuadShaderMaterial = m_MaterialManager->CreateMaterial(*pVulkan, *pBlitQuadShader, pVulkan->m_SwapchainImageCount,
        [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
        {
            if (texName == "Diffuse")
            {
                return { &m_UpscaledImageResult };
            }
            else if (texName == "Overlay")
            {
                return { &m_RenderPassData[RP_HUD].RenderTarget[0].m_ColorAttachments[0] };
            }
            return {};
        },
        [this](const std::string& bufferName) -> tPerFrameVkBuffer
        {
            return {};
        }
        );

    m_BlitQuadDrawable = std::make_unique<Drawable>(*pVulkan, std::move(blitQuadShaderMaterial));
    if (!m_BlitQuadDrawable->Init(m_RenderPassData[RP_BLIT].RenderPass, sRenderPassNames[RP_BLIT], std::move(blitQuadMesh)))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitCommandBuffers()
//-----------------------------------------------------------------------------
{
    LOGI("*******************************");
    LOGI("Initializing Command Buffers...");
    LOGI("*******************************");

    Vulkan* const pVulkan = GetVulkan();

    auto GetPassName = [](uint32_t whichPass)
    {
        if (whichPass >= sRenderPassNames.size())
        {
            LOGE("GetPassName() called with unknown pass (%d)!", whichPass);
            return "RP_UNKNOWN";
        }

        return sRenderPassNames[whichPass];
    };

    m_RenderPassData[RP_SCENE].PassCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_HUD].PassCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_HUD].ObjectsCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_BLIT].PassCmdBuffer.resize(pVulkan->m_SwapchainImageCount);
    m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.resize(pVulkan->m_SwapchainImageCount);

    char szName[256];
    const VkCommandBufferLevel CmdBuffLevel = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        for (uint32_t whichBuffer = 0; whichBuffer < m_RenderPassData[whichPass].PassCmdBuffer.size(); whichBuffer++)
        {
            // The Pass Command Buffer => Primary
            sprintf(szName, "Primary (%s; Buffer %d of %d)", GetPassName(whichPass), whichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
            {
                return false;
            }

            // Model => Secondary
            sprintf(szName, "Model (%s; Buffer %d of %d)", GetPassName(whichPass), whichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].Initialize(pVulkan, szName, CmdBuffLevel))
            {
                return false;
            }
        }
    }

    LOGI("Creating Graph Pipeline Command Lists");
    if (m_IsGraphPipelinesSupported)
    {
        m_GraphPipelineCommandLists.resize(NUM_VULKAN_BUFFERS);
        for (auto& graphPipelineCommandList : m_GraphPipelineCommandLists)
        {
            if (!graphPipelineCommandList.Initialize(GetVulkan(), "Graph Pipeline CMD Buffer", VK_COMMAND_BUFFER_LEVEL_PRIMARY, Vulkan::eDataGraphQueue))
            {
                return false;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitLocalSemaphores()
//-----------------------------------------------------------------------------
{
    LOGI("********************************");
    LOGI("Initializing Local Semaphores...");
    LOGI("********************************");

    const VkSemaphoreCreateInfo SemaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        VkResult retVal = vkCreateSemaphore(GetVulkan()->m_VulkanDevice, &SemaphoreInfo, NULL, &m_RenderPassData[whichPass].PassCompleteSemaphore);
        if (!CheckVkError("vkCreateSemaphore()", retVal))
        {
            return false;
        }
    }

    // Create the graph pipeline semaphore
    {
        VkResult retVal = vkCreateSemaphore(GetVulkan()->m_VulkanDevice, &SemaphoreInfo, NULL, &m_GraphPipelinePassCompleteSemaphore);
        if (!CheckVkError("vkCreateSemaphore()", retVal))
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::BuildCmdBuffers()
//-----------------------------------------------------------------------------
{
    LOGI("***************************");
    LOGI("Building Command Buffers...");
    LOGI("****************************");

    Vulkan* const pVulkan = GetVulkan();

    // Begin recording
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        auto& renderPassData         = m_RenderPassData[whichPass];
        bool  bisSwapChainRenderPass = whichPass == RP_BLIT;

        for (uint32_t whichBuffer = 0; whichBuffer < renderPassData.ObjectsCmdBuffer.size(); whichBuffer++)
        {
            auto& cmdBufer = renderPassData.ObjectsCmdBuffer[whichBuffer];

            uint32_t targetWidth  = bisSwapChainRenderPass ? pVulkan->m_SurfaceWidth : renderPassData.RenderTarget[0].m_Width;
            uint32_t targetHeight = bisSwapChainRenderPass ? pVulkan->m_SurfaceHeight : renderPassData.RenderTarget[0].m_Height;

            VkViewport viewport = {};
            viewport.x          = 0.0f;
            viewport.y          = 0.0f;
            viewport.width      = (float)targetWidth;
            viewport.height     = (float)targetHeight;
            viewport.minDepth   = 0.0f;
            viewport.maxDepth   = 1.0f;

            VkRect2D scissor      = {};
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;
            scissor.extent.width  = targetWidth;
            scissor.extent.height = targetHeight;

            // Set up some values that change based on render pass
            VkRenderPass  whichRenderPass  = renderPassData.RenderPass;
            VkFramebuffer whichFramebuffer = bisSwapChainRenderPass ? pVulkan->m_SwapchainBuffers[whichBuffer].framebuffer : renderPassData.RenderTarget[0].m_FrameBuffer;

            // Objects (can render into any pass except Blit)
            if (!cmdBufer.Begin(whichFramebuffer, whichRenderPass, bisSwapChainRenderPass))
            {
                return false;
            }
            vkCmdSetViewport(cmdBufer.m_VkCommandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(cmdBufer.m_VkCommandBuffer, 0, 1, &scissor);
        }
    }
    
    // Scene drawables
    for (const auto& sceneDrawable : m_SceneDrawables)
    {
        AddDrawableToCmdBuffers(sceneDrawable, m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.size()));
    }

    // Blit quad drawable
    AddDrawableToCmdBuffers(*m_BlitQuadDrawable.get(), m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.size()));

    // End recording
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        auto& renderPassData = m_RenderPassData[whichPass];

        for (uint32_t whichBuffer = 0; whichBuffer < renderPassData.ObjectsCmdBuffer.size(); whichBuffer++)
        {
            auto& cmdBufer = renderPassData.ObjectsCmdBuffer[whichBuffer];
            if (!cmdBufer.End())
            {
                return false;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::UpdateGui()
//-----------------------------------------------------------------------------
{
    if (m_Gui)
    {
        m_Gui->Update();
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::Begin("FPS", (bool*)nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::Text("FPS: %.1f", m_CurrentFPS);
            ImGui::Text("Camera [%f, %f, %f]", m_Camera.Position().x, m_Camera.Position().y, m_Camera.Position().z);

            ImGui::Separator();

            ImGui::BeginDisabled(!m_IsGraphPipelinesSupported);
            ImGui::Checkbox("Upscaling Enabled", &m_ShouldUpscale);
            ImGui::EndDisabled();

            ImGui::Separator();

            ImGui::DragFloat3("Sun Dir", &m_LightUniformData.LightDirection.x, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat3("Sun Color", &m_LightUniformData.LightColor.x, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Sun Intensity", &m_LightUniformData.LightColor.w, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat3("Ambient Color", &m_LightUniformData.AmbientColor.x, 0.01f, 0.0f, 1.0f);

            for (int i = 0; i < NUM_SPOT_LIGHTS; i++)
            {
                std::string childName = std::string("Spot Light ").append(std::to_string(i+1));
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", childName.c_str());

                if (ImGui::CollapsingHeader(childName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
                {
                    ImGui::PushID(i);

                    ImGui::DragFloat3("Pos", &m_LightUniformData.SpotLights_pos[i].x, 0.1f);
                    ImGui::DragFloat3("Dir", &m_LightUniformData.SpotLights_dir[i].x, 0.01f, -1.0f, 1.0f);
                    ImGui::DragFloat3("Color", &m_LightUniformData.SpotLights_color[i].x, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Intensity", &m_LightUniformData.SpotLights_color[i].w, 0.1f, 0.0f, 100.0f);

                    ImGui::PopID();
                }

                ImDrawList* list = ImGui::GetWindowDrawList();

                glm::vec3 LightDirNotNormalized = m_LightUniformData.SpotLights_dir[i];
                LightDirNotNormalized = glm::normalize(LightDirNotNormalized);
                m_LightUniformData.SpotLights_dir[i] = glm::vec4(LightDirNotNormalized, 0.0f);
            }

            glm::vec3 LightDirNotNormalized   = m_LightUniformData.LightDirection;
            LightDirNotNormalized             = glm::normalize(LightDirNotNormalized);
            m_LightUniformData.LightDirection = glm::vec4(LightDirNotNormalized, 0.0f);
        }
        ImGui::End();

        return;
    }
}

//-----------------------------------------------------------------------------
bool Application::UpdateUniforms(uint32_t whichBuffer)
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    // Vert data
    {
        glm::mat4 LocalModel = glm::mat4(1.0f);
        LocalModel           = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        LocalModel           = glm::scale(LocalModel, glm::vec3(1.0f));
        glm::mat4 LocalMVP   = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix() * LocalModel;

        m_ObjectVertUniformData.MVPMatrix   = LocalMVP;
        m_ObjectVertUniformData.ModelMatrix = LocalModel;
        UpdateUniformBuffer(pVulkan, m_ObjectVertUniform, m_ObjectVertUniformData);
    }

    // Frag data
    for (auto& [hash, objectUniform] : m_ObjectFragUniforms)
    {
        UpdateUniformBuffer(pVulkan, objectUniform.objectFragUniform, objectUniform.objectFragUniformData);
    }

    // Light data
    {
        glm::mat4 CameraViewInv       = glm::inverse(m_Camera.ViewMatrix());
        glm::mat4 CameraProjection    = m_Camera.ProjectionMatrix();
        glm::mat4 CameraProjectionInv = glm::inverse(CameraProjection);

        m_LightUniformData.ProjectionInv     = CameraProjectionInv;
        m_LightUniformData.ViewInv           = CameraViewInv;
        m_LightUniformData.ViewProjectionInv = glm::inverse(CameraProjection * m_Camera.ViewMatrix());
        m_LightUniformData.ProjectionInvW    = glm::vec4(CameraProjectionInv[0].w, CameraProjectionInv[1].w, CameraProjectionInv[2].w, CameraProjectionInv[3].w);
        m_LightUniformData.CameraPos         = glm::vec4(m_Camera.Position(), 0.0f);

        UpdateUniformBuffer(pVulkan, m_LightUniform, m_LightUniformData);
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();

    // Obtain the next swap chain image for the next frame.
    auto currentVulkanBuffer = pVulkan->SetNextBackBuffer();
    uint32_t whichBuffer     = currentVulkanBuffer.idx;

    // ********************************
    // Application Draw() - Begin
    // ********************************

    UpdateGui();

    // Update camera
    m_Camera.UpdateController(fltDiffTime * 0.1f, *m_CameraController);
    m_Camera.UpdateMatrices();
 
    // Update uniform buffers with latest data
    UpdateUniforms(whichBuffer);

    // First time through, wait for the back buffer to be ready
    std::span<const VkSemaphore> pWaitSemaphores = { &currentVulkanBuffer.semaphore, 1 };

    std::array<VkPipelineStageFlags, 1> waitDstStageMasks = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    const bool isUpscalingActive = m_IsGraphPipelinesSupported && m_ShouldUpscale;

    // RP_SCENE
    {
        BeginRenderPass(whichBuffer, RP_SCENE, currentVulkanBuffer.swapchainPresentIdx);
        AddPassCommandBuffer(whichBuffer, RP_SCENE);
        EndRenderPass(whichBuffer, RP_SCENE);

        // Before finishing the scene cmd buffer, copy its color render target contents to the tensor buffer if upscaling 
        // is active, otherwise blit them directly to the upscaled image.
        if (isUpscalingActive)
        {
            CopyImageToTensor(
                m_RenderPassData[RP_SCENE].PassCmdBuffer[whichBuffer],
                m_RenderPassData[RP_SCENE].RenderTarget.m_RenderTargets[0].m_ColorAttachments[0],
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                m_InputTensor);
        }
        else
        {
            CopyImageToImageBlit(
                m_RenderPassData[RP_SCENE].PassCmdBuffer[whichBuffer], 
                m_RenderPassData[RP_SCENE].RenderTarget.m_RenderTargets[0].m_ColorAttachments[0], 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                m_UpscaledImageResult,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // Submit the commands to the queue.
        SubmitRenderPass(whichBuffer, RP_SCENE, pWaitSemaphores, waitDstStageMasks, { &m_RenderPassData[RP_SCENE].PassCompleteSemaphore,1 });
        pWaitSemaphores      = { &m_RenderPassData[RP_SCENE].PassCompleteSemaphore, 1 };
        waitDstStageMasks[0] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    }

    // Data Graph preparation + dispatch for Upscaling
    if (isUpscalingActive)
    {
        const auto& data_graph_extension = GetVulkan()->GetExtension<ExtensionHelper::Ext_VK_ARM_data_graph>();
        assert(data_graph_extension != nullptr);

        m_GraphPipelineCommandLists[whichBuffer].Begin();

        vkCmdBindPipeline(
            m_GraphPipelineCommandLists[whichBuffer].m_VkCommandBuffer,
            VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM,
            m_GraphPipelineInstance.graphPipeline);

        vkCmdBindDescriptorSets(
            m_GraphPipelineCommandLists[whichBuffer].m_VkCommandBuffer, 
            VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM, 
            m_GraphPipelineInstance.pipelineLayout,
            0, 
            1, 
            &m_TensorDescriptorSet,
            0,
            NULL);

        VkDataGraphPipelineDispatchInfoARM dispatchInfo;
        dispatchInfo.sType = VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_DISPATCH_INFO_ARM;
        dispatchInfo.flags = 0;

        data_graph_extension->m_vkCmdDispatchDataGraphARM(
            m_GraphPipelineCommandLists[whichBuffer].m_VkCommandBuffer, 
            m_GraphPipelineInstance.graphSession,
            &dispatchInfo);

        m_GraphPipelineCommandLists[whichBuffer].End();
        m_GraphPipelineCommandLists[whichBuffer].QueueSubmit(pWaitSemaphores[0], waitDstStageMasks[0], m_GraphPipelinePassCompleteSemaphore);
        pWaitSemaphores      = { &m_GraphPipelinePassCompleteSemaphore, 1 };
        waitDstStageMasks[0] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT }; // Should be VK_PIPELINE_STAGE_2_DATA_GRAPH_BIT_ARM, but need to update framework
                                                                       // to support VK_PIPELINE_STAGE_2.
    }

    // RP_HUD
    VkCommandBuffer guiCommandBuffer = VK_NULL_HANDLE;
    if (m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        guiCommandBuffer = GetGui()->Render(whichBuffer, m_RenderPassData[RP_HUD].RenderTarget[0].m_FrameBuffer);
        if (guiCommandBuffer != VK_NULL_HANDLE)
        {
            BeginRenderPass(whichBuffer, RP_HUD, currentVulkanBuffer.swapchainPresentIdx);
            vkCmdExecuteCommands(m_RenderPassData[RP_HUD].PassCmdBuffer[whichBuffer].m_VkCommandBuffer, 1, &guiCommandBuffer);
            EndRenderPass(whichBuffer, RP_HUD);

            // Submit the commands to the queue.
            SubmitRenderPass(whichBuffer, RP_HUD, pWaitSemaphores, waitDstStageMasks, { &m_RenderPassData[RP_HUD].PassCompleteSemaphore,1 });
            pWaitSemaphores      = { &m_RenderPassData[RP_HUD].PassCompleteSemaphore,1 };
            waitDstStageMasks[0] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        }
    }

    // Blit Results to the screen
    {
        if (!m_RenderPassData[RP_BLIT].PassCmdBuffer[whichBuffer].Reset())
        {
            LOGE("Pass (%d) command buffer Reset() failed !", RP_BLIT);
        }

        if (!m_RenderPassData[RP_BLIT].PassCmdBuffer[whichBuffer].Begin())
        {
            LOGE("Pass (%d) command buffer Begin() failed !", RP_BLIT);
        }

        // Before begining the blit render pass, copy the output tensor contents to the upscaled image if upscaling
        // is active, otherwise nothing needs to be done as the scene pass should have already blit its contents directly
        // into the upscale image.
        if (isUpscalingActive)
        {
            CopyTensorToImage(
                m_RenderPassData[RP_BLIT].PassCmdBuffer[whichBuffer],
                m_UpscaledImageResult,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                m_OutputTensor);
        }

        BeginRenderPass(whichBuffer, RP_BLIT, currentVulkanBuffer.swapchainPresentIdx, false);
        AddPassCommandBuffer(whichBuffer, RP_BLIT);
        EndRenderPass(whichBuffer, RP_BLIT);

        // Submit the commands to the queue.
        SubmitRenderPass(whichBuffer, RP_BLIT, pWaitSemaphores, waitDstStageMasks, { &m_RenderPassData[RP_BLIT].PassCompleteSemaphore,1 }, currentVulkanBuffer.fence);
        pWaitSemaphores      = { &m_RenderPassData[RP_BLIT].PassCompleteSemaphore,1 };
        waitDstStageMasks[0] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    }

    // Queue is loaded up, tell the driver to start processing
    pVulkan->PresentQueue(pWaitSemaphores, currentVulkanBuffer.swapchainPresentIdx);

    // ********************************
    // Application Draw() - End
    // ********************************
}

//-----------------------------------------------------------------------------
void Application::BeginRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass, uint32_t WhichSwapchainImage, bool beginCmdBuffer)
//-----------------------------------------------------------------------------
{
    Vulkan* const pVulkan = GetVulkan();
    auto& renderPassData         = m_RenderPassData[whichPass];
    bool  bisSwapChainRenderPass = whichPass == RP_BLIT;

    if (beginCmdBuffer)
    {
        if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Reset())
        {
            LOGE("Pass (%d) command buffer Reset() failed !", whichPass);
        }

        if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Begin())
        {
            LOGE("Pass (%d) command buffer Begin() failed !", whichPass);
        }
    }
    
    VkFramebuffer framebuffer = nullptr;
    switch (whichPass)
    {
    case RP_SCENE:
        framebuffer = m_RenderPassData[whichPass].RenderTarget[0].m_FrameBuffer;
        break;
    case RP_HUD:
        framebuffer = m_RenderPassData[whichPass].RenderTarget[0].m_FrameBuffer;
        break;
    case RP_BLIT:
        framebuffer = pVulkan->m_SwapchainBuffers[WhichSwapchainImage].framebuffer;
        break;
    default:
        framebuffer = nullptr;
        break;
    }

    assert(framebuffer != nullptr);

    VkRect2D passArea = {};
    passArea.offset.x = 0;
    passArea.offset.y = 0;
    passArea.extent.width  = bisSwapChainRenderPass ? pVulkan->m_SurfaceWidth  : renderPassData.RenderTarget[0].m_Width;
    passArea.extent.height = bisSwapChainRenderPass ? pVulkan->m_SurfaceHeight : renderPassData.RenderTarget[0].m_Height;

    TextureFormat                  swapChainColorFormat = pVulkan->m_SurfaceFormat;
    auto                           swapChainColorFormats = std::span<const TextureFormat>({ &swapChainColorFormat, 1 });
    TextureFormat                  swapChainDepthFormat = pVulkan->m_SwapchainDepth.format;
    std::span<const TextureFormat> colorFormats         = bisSwapChainRenderPass ? swapChainColorFormats : m_RenderPassData[whichPass].RenderTarget[0].m_pLayerFormats;
    TextureFormat                  depthFormat          = bisSwapChainRenderPass ? swapChainDepthFormat : m_RenderPassData[whichPass].RenderTarget[0].m_DepthFormat;

    VkClearColorValue clearColor = { renderPassData.PassSetup.ClearColor[0], renderPassData.PassSetup.ClearColor[1], renderPassData.PassSetup.ClearColor[2], renderPassData.PassSetup.ClearColor[3] };

    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].BeginRenderPass(
        passArea,
        0.0f,
        1.0f,
        { &clearColor , 1 },
        (uint32_t)colorFormats.size(),
        depthFormat != TextureFormat::UNDEFINED,
        m_RenderPassData[whichPass].RenderPass,
        bisSwapChainRenderPass,
        framebuffer,
        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}


//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffer(uint32_t whichBuffer, RENDER_PASS whichPass)
//-----------------------------------------------------------------------------
{
    if (m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].m_NumDrawCalls)
    {
        vkCmdExecuteCommands(m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].m_VkCommandBuffer, 1, &m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].m_VkCommandBuffer);
    }
}

//-----------------------------------------------------------------------------
void Application::EndRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass)
//-----------------------------------------------------------------------------
{
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].EndRenderPass();
}

//-----------------------------------------------------------------------------
void Application::SubmitRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass, const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, std::span<VkSemaphore> SignalSemaphores, VkFence CompletionFence)
//-----------------------------------------------------------------------------
{
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].End();
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].QueueSubmit(WaitSemaphores, WaitDstStageMasks, SignalSemaphores, CompletionFence);
}
