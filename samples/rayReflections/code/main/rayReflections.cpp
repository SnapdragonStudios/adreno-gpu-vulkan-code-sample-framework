//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "rayReflections.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "gui/imguiVulkan.hpp"
#include "main/applicationEntrypoint.hpp"
#include "material/computable.hpp"
#include "material/drawable.hpp"
#include "material/material.hpp"
#include "material/materialManager.hpp"
#include "material/shader.hpp"
#include "material/shaderDescription.hpp"
#include "material/shaderManager.hpp"
#include "memory/memoryManager.hpp"
#include "memory/vulkan/bufferObject.hpp"
#include "memory/vulkan/indexBufferObject.hpp"
#include "memory/vulkan/vertexBufferObject.hpp"
#include "mesh/instanceGenerator.hpp"
#include "mesh/meshHelper.hpp"
#include "system/math_common.hpp"
#include "texture/vulkan/textureManager.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/TextureFuncts.h"
#include "vulkanRT/vulkanRT.hpp"
#include "vulkanRT/sceneRT.hpp"
#include "vulkanRT/traceable.hpp"
#include "imgui.h"

#include <algorithm>
#include <cassert>

#include "vulkanRT/meshObjectRT.hpp"

VAR(bool, gRenderHud, true, kVariableNonpersistent);

// General Settings
VAR(glm::vec3, gCameraStartPos, glm::vec3(0.0f, 2.0f, 5.0f), kVariableNonpersistent);
VAR(glm::vec3, gStartCameraLook, glm::vec3(0.0f, 2.0f, 0.0f), kVariableNonpersistent);

VAR(float, gFOV, PI_DIV_4, kVariableNonpersistent);
VAR(float, gNearPlane, 0.01f, kVariableNonpersistent);
VAR(float, gFarPlane, 50.0f, kVariableNonpersistent);

// Objects in the scene
VAR(uint32_t, gNumObjects, 2, kVariableNonpersistent);
VAR(char*, gObjectName, "Media/Meshes/UVSphere_Separate.gltf", kVariableNonpersistent);
VAR(char*, gObjectDiffuse, "stub_green", kVariableNonpersistent);
VAR(glm::vec3, gObjectScale, glm::vec3(1.0f, 1.0f, 1.0f), kVariableNonpersistent);
VAR(float, gObjectHeight, 1.2f, kVariableNonpersistent);
VAR(float, gSpacingRadius, 2.0f, kVariableNonpersistent);

VAR(char*, gFloorName, "Media/Meshes/Floor_Separate.gltf", kVariableNonpersistent);
VAR(glm::vec3, gFloorPos, glm::vec3(0.0f, 0.0f, 0.0f), kVariableNonpersistent);
VAR(glm::vec3, gFloorScale, glm::vec3(8.0f, 8.0f, 8.0f), kVariableNonpersistent);
VAR(char*, gFloorDiffuse, "bbb_splash", kVariableNonpersistent);

// Environment Settings
VAR(glm::vec3, gLightDirA, glm::vec3(-0.5f, -1.0f, -0.7f), kVariableNonpersistent);
VAR(glm::vec3, gLightDirB, glm::vec3(-0.5f, -1.0f, -0.7f), kVariableNonpersistent);
VAR(float, gLightDirChangeRate, 0.0f, kVariableNonpersistent);

VAR(glm::vec4, gLightColorA, glm::vec4(1.0f, 1.0f, 1.0f, 128.0f), kVariableNonpersistent);
VAR(glm::vec4, gLightColorB, glm::vec4(1.0f, 1.0f, 1.0f, 128.0f), kVariableNonpersistent);
VAR(float, gLightColorChangeRate, 0.0f, kVariableNonpersistent);

// Log Settings

// Misc. Settings
VAR(float, gRayMinDistance, 0.05f, kVariableNonpersistent);
VAR(float, gRayMaxDistance, 10000.0f, kVariableNonpersistent);
VAR(uint32_t, gSphereRayBounces, 2, kVariableNonpersistent);
VAR(uint32_t, gFloorRayBounces, 2, kVariableNonpersistent);
VAR(float, gRayBlendAmount, 0.5f, kVariableNonpersistent);

VAR(float, gShadowAmount, 0.25f, kVariableNonpersistent);
VAR(float, gMinNormDotLight, 0.0f, kVariableNonpersistent);

VAR(glm::vec4, gClearColor, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f), kVariableNonpersistent);

static const char* const sRenderPassNames[NUM_RENDER_PASSES] = { "RP_GBUFFER", "RP_RAYTRACE", "RP_LIGHT", "RP_HUD", "RP_BLIT" };
static_assert(RP_GBUFFER == 0, "Check order of sRenderPassNames");
static_assert(RP_BLIT == 4, "Check order of sRenderPassNames");
static_assert(NUM_RENDER_PASSES == sizeof(sRenderPassNames) / sizeof(sRenderPassNames[0]), "mismatched sRenderPassNames");

uint32_t guiShiftPressed = 0;
uint32_t guiUpPressed = 0;
uint32_t guiDownPressed = 0;
uint32_t guiLeftPressed = 0;
uint32_t guiRightPressed = 0;

uint32_t guiAPressed = 0;
uint32_t guiSPressed = 0;
uint32_t guiWPressed = 0;
uint32_t guiDPressed = 0;
uint32_t guiQPressed = 0;
uint32_t guiEPressed = 0;

uint32_t guiSpacePressedTime = 0;
uint32_t guiSpacePressedThisFrame = 0;

#define KEY_THIS_FRAME_TIME     250


//-----------------------------------------------------------------------------
float L_Lerp(float t, float a, float b)
//-----------------------------------------------------------------------------
{
    return a + t * (b - a);
}

//-----------------------------------------------------------------------------
glm::vec3 L_Lerp(float t, glm::vec3 a, glm::vec3 b)
//-----------------------------------------------------------------------------
{
    return a + t * (b - a);
}

//-----------------------------------------------------------------------------
glm::vec4 L_Lerp(float t, glm::vec4 a, glm::vec4 b)
//-----------------------------------------------------------------------------
{
    return a + t * (b - a);
}


//
// Implementation of the Application entrypoint (called by the framework)
// Construct the Application class
//
//-----------------------------------------------------------------------------
FrameworkApplicationBase* Application_ConstructApplication()
//-----------------------------------------------------------------------------
{
    return new Application();
}

//-----------------------------------------------------------------------------
Application::Application()
//-----------------------------------------------------------------------------
    : ApplicationHelperBase()
    , m_bEncodeSRGB(false)
    , m_TotalDrawCalls(0)
    , m_TotalTriangles(0)
    , m_vulkanRT(*GetVulkan())
{
    // Pass Semaphores
    m_PassCompleteSemaphore.fill(VK_NULL_HANDLE);

    // Render passes
    m_RenderPass.fill(VK_NULL_HANDLE);

    // Want to track if the scene data size changes
    m_SceneDataArray.clear();
    m_SceneDataInternalArray.clear();

    m_SceneTextures.clear();

    m_RayMeshArray.clear();

    m_RayIndicesArray.clear();

    m_LastUpdateTime = 0;
    m_LightChangePaused = false;
    m_LightDirUpdateTime = 0;
    m_LightDirReverse = false;
    m_LightColorUpdateTime = 0;
    m_LightColorReverse = false;
}


//-----------------------------------------------------------------------------
Application::~Application()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    // Pass Semaphores
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        if (m_PassCompleteSemaphore[WhichPass] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(pVulkan->m_VulkanDevice, m_PassCompleteSemaphore[WhichPass], NULL);
        }
        m_PassCompleteSemaphore[WhichPass] = VK_NULL_HANDLE;
    }

    // Passes
    for (auto& pass : m_RenderPass)
    {
        vkDestroyRenderPass(pVulkan->m_VulkanDevice, pass, nullptr);
        pass = VK_NULL_HANDLE;
    }

    // Compute

    // Textures
    ReleaseTexture(*pVulkan, &m_TexWhite);
    ReleaseTexture(*pVulkan, &m_DefaultNormal);

    // ReleaseTexture(*pVulkan, &m_TexBunnySplash);

    for (auto& OneMeshBuffer : m_RayMeshBuffers)
    {
        OneMeshBuffer->Destroy();
    }
    m_RayMeshBuffers.clear();
    m_RayMeshVkBuffers.clear();

    for (auto& OneIndiceBuffer : m_RayIndiceBuffers)
    {
        OneIndiceBuffer->Destroy();
    }
    m_RayIndiceBuffers.clear();
    m_RayIndiceVkBuffers.clear();

}

//-----------------------------------------------------------------------------
int Application::PreInitializeSelectSurfaceFormat(std::span<const SurfaceFormat> formats)
//-----------------------------------------------------------------------------
{
    // On Snapdragon if the surfaceflinger has to do the rotation to the display native orientation then it will do it at 8bit colordepth.
    // To avoid this we need to enable the 'pre-rotation' of the display (and the use of VK_QCOM_render_pass_transform so we dont have to rotate our buffers/passes manually).
    GetVulkan()->m_UseRenderPassTransform = true;

    // We want to select a SRGB output format (if one exists)
    int index = 0;
    for (const auto& format : formats)
    {
        // HLM doesn't support SRGB format
        if (gRunOnHLM)
        {
            if (format.format == TextureFormat::B8G8R8A8_UNORM)
                return index;
        }
        else
        {
            if (format.format == TextureFormat::B8G8R8A8_SRGB)
                return index;
        }
        ++index;
    }
    return -1;
}

//-----------------------------------------------------------------------------
void Application::PreInitializeSetVulkanConfiguration(Vulkan::AppConfiguration& config)
//-----------------------------------------------------------------------------
{
    bool rayQueryOnly = true;
    ApplicationHelperBase::PreInitializeSetVulkanConfiguration(config);
    m_vulkanRT.RegisterRequiredVulkanLayerExtensions(config, rayQueryOnly);
}

//-----------------------------------------------------------------------------
bool Application::Initialize( uintptr_t windowHandle, uintptr_t hInstance )
//-----------------------------------------------------------------------------
{
    if (!ApplicationHelperBase::Initialize( windowHandle, hInstance ))
    {
        return false;
    }

    m_SphereCamera.SetScreenSize(gRenderWidth, gRenderHeight);
    m_SphereCamera.SetPosition(gCameraStartPos);
    m_SphereCamera.SetLookAt(gStartCameraLook);

    // Camera
    m_SphereCamera.SetPosition(gCameraStartPos);
    m_SphereCamera.SetAspect(float(gRenderWidth) / float(gRenderHeight));
    m_SphereCamera.SetFov(gFOV);
    m_SphereCamera.SetClipPlanes(gNearPlane, gFarPlane);

    // The Skybox
    m_SkyboxScale = gFarPlane * 0.95f;

    // Camera Controller
    if (!ApplicationHelperBase::InitCamera())
    {
        return false;
    }

    // Set the current surface format
    m_RequestedSurfaceFormat = {GetVulkan()->m_SurfaceFormat, GetVulkan()->m_SurfaceColorSpace};
    m_bEncodeSRGB = FormatIsSrgb(GetVulkan()->m_SurfaceFormat) == false;   // if we have an srgb buffer then the output doesnt need to be encoded (hardware will do it for us)

    if (!LoadConstantShaders())
        return false;

    if (!LoadTextures())
        return false;

    if (!CreateRenderTargets())
        return false;

    if (!InitShadowMap())
        return false;

    if (!InitLighting())
        return false;

    if (!InitUniforms())
        return false;

    if (!InitAllRenderPasses())
        return false;

    if (!InitMaterials())
        return false;

    if (!InitGui(windowHandle))
        return false;

    if (!LoadMeshObjects())
        return false;

    if (!LoadRayTracingObjects())
        return false;

    if (!InitCommandBuffers())
        return false;

    if (!InitDrawables())
        return false;

    if (!CreateRayInputs())
        return false;
    
    // if (!InitSceneData())
    //     return false;
    
    if (!LoadDynamicShaders())
        return false;

    if (!LoadDynamicRayTracingObjects())
        return false;

    if (!gRunOnHLM && !InitHdr())
        return false;

    if (!InitLocalSemaphores())
        return false;

    if (!BuildCmdBuffers())
        return false;

    LOGE("Initalize lighting!!");

    LOGE("Initalize workers!!");

    m_GameThreadWorker.Initialize("GameThreadWorker", 1);

    m_CompletedThreadOutput = {};

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadMeshObjects()
//-----------------------------------------------------------------------------
{
    //
    // Create the 'Blit' drawable mesh.
    // Fullscreen quad that does the final composite (hud and scene) for output.
    LOGI("Creating Blit mesh...");
    MeshObject blitMesh;
    MeshHelper::CreateScreenSpaceMesh(GetVulkan()->GetMemoryManager(), 0, &blitMesh);

    const auto* pBlitShader = m_ShaderManager->GetShader("Blit");
    assert(pBlitShader);
    auto blitShaderMaterial = m_MaterialManager->CreateMaterial(*GetVulkan(), *pBlitShader, NUM_VULKAN_BUFFERS,
        [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo {
            if (texName == "Diffuse") {
                return { &m_MainRT[0].m_ColorAttachments[0] };
            }
            else if (texName == "Overlay") {
                return { &m_HudRT[0].m_ColorAttachments[0] };
            }
            else if (texName == "Bloom") {
                return { &m_TexWhite };
            }
            assert(0);
            return {};
        },
        [this](const std::string& bufferName) -> tPerFrameVkBuffer {
            //BlitFragCB
            return { m_BlitFragUniform[0].buf.GetVkBuffer() };
        }
        );

    m_BlitDrawable = std::make_unique<Drawable>(*GetVulkan(), std::move(blitShaderMaterial));
    if (!m_BlitDrawable->Init( m_RenderPass[RP_BLIT], sRenderPassNames[RP_BLIT], std::move(blitMesh)))
    {
        LOGE("Error Creating Blit drawable...");
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadRayTracingObjects()
//-----------------------------------------------------------------------------
{
    // Load the Geometry for Ray Tracing! and populate m_sceneRayTracable
    if (!m_vulkanRT.Init())
    {
        LOGE("Vulkan Ray Tracing Extension was not available...exiting");
        return false;
    }

    // Might be overkill, but may eventually need different scales in each axis
    std::vector<MeshObjectIntermediate> candidateMeshes;

    // Load the objects in scene
    float StepRadians = PI_MUL_2 / (float)gNumObjects;
    for (uint32_t WhichObject = 0; WhichObject < gNumObjects; WhichObject++)
    {
        float OneX = gSpacingRadius * cosf((float)WhichObject * StepRadians);
        float OneY = gObjectHeight;
        float OneZ = gSpacingRadius * sinf((float)WhichObject * StepRadians);

        // This is really the same object, just load it multiple times and will instance later
        std::vector<MeshObjectIntermediate> MeshObject = MeshObjectIntermediate::LoadGLTF(*m_AssetManager, gObjectName, false, gObjectScale);
        for (MeshObjectIntermediate& OneMesh : MeshObject)
        {
            OneMesh.m_Transform[3] = glm::vec4(OneX, OneY, OneZ, 1.0f);
            OneMesh.m_Materials[0].diffuseFilename = gObjectDiffuse;
            OneMesh.m_Materials[0].bumpFilename = std::string("stub_normal");
        }
        candidateMeshes.insert(candidateMeshes.end(), std::make_move_iterator(MeshObject.begin()), std::make_move_iterator(MeshObject.end()));
        MeshObject.erase(MeshObject.begin(), MeshObject.end());
    }

    // Load the floor/base of the scene
    {
        // LOGI("Loading Ray Tracing mesh object: %s...", gFloorName);
        std::vector<MeshObjectIntermediate> MeshObject = MeshObjectIntermediate::LoadGLTF(*m_AssetManager, gFloorName, false, gFloorScale);
        for (MeshObjectIntermediate& OneMesh : MeshObject)
        {
            if (OneMesh.m_Materials.size() < 1)
                OneMesh.m_Materials.resize(1);

            OneMesh.m_Transform = glm::rotate(OneMesh.m_Transform, -PI_DIV_2, glm::vec3(1.0f, 0.0f, 0.0f));
            OneMesh.m_Transform = glm::rotate(OneMesh.m_Transform, PI, glm::vec3(0.0f, 1.0f, 0.0f));

            OneMesh.m_Transform[3] = glm::vec4(gFloorPos, 1.0f);
            OneMesh.m_Materials[0].diffuseFilename = gFloorDiffuse;
            OneMesh.m_Materials[0].bumpFilename = std::string("stub_normal");
        }
        candidateMeshes.insert(candidateMeshes.end(), std::make_move_iterator(MeshObject.begin()), std::make_move_iterator(MeshObject.end()));
        MeshObject.erase(MeshObject.begin(), MeshObject.end());
    }

    // (optionally) go through and find meshes that match each other (automated instancing).
    auto instancedMeshes = MeshInstanceGenerator::NullFindInstances( std::move(candidateMeshes) );
    candidateMeshes.clear();

    // Need the mesh data for each object to pass into ray tracer as storage buffer
#define FILL_4_FROM_2(Dst, Src, Pad) {\
                    Dst[0] = Src[0];\
                    Dst[1] = Src[1];\
                    Dst[2] = Pad; \
                    Dst[3] = Pad;\
    }

#define FILL_4_FROM_3(Dst, Src, Pad) {\
                    Dst[0] = Src[0];\
                    Dst[1] = Src[1];\
                    Dst[2] = Src[2]; \
                    Dst[3] = Pad;\
    }

    for (const auto& OneMesh : instancedMeshes)
    {
        // ****************************
        // Vertices
        // ****************************
        {
            RayObject NewRayObject;
            for (const auto& OneVertex : OneMesh.mesh.m_VertexBuffer)
            {
                RayVertex NewRayVert;
                FILL_4_FROM_3(NewRayVert.position, OneVertex.position, 1.0f);   // THIS MUST HAVE 1.0 IN W OR TRANSFORM WILL NOT WORK :(
                FILL_4_FROM_3(NewRayVert.normal, OneVertex.normal, 0.0f);
                // FILL_4_FROM_3(NewRayVert.color, OneVertex.color, 0.0f);
                FILL_4_FROM_2(NewRayVert.uv0, OneVertex.uv0, 0.0f);
                // FILL_4_FROM_3(NewRayVert.tangent, OneVertex.tangent, 0.0f);
                // FILL_4_FROM_3(NewRayVert.bitangent, OneVertex.bitangent, 0.0f);

                NewRayObject.push_back(NewRayVert);
            }

            m_RayMeshArray.push_back(NewRayObject);

            // Create the buffer for this object
            BufferVulkan* pNewBuffer = new BufferVulkan;
            BufferUsageFlags Usage = BufferUsageFlags::Storage | BufferUsageFlags::ShaderDeviceAddress;
            if (!pNewBuffer || !pNewBuffer->Initialize(&GetVulkan()->GetMemoryManager(), NewRayObject.size() * sizeof(RayVertex), Usage, NewRayObject.data()))
            {
                LOGE("Error! Unable to allocate %d * %d bytes for ray shader vertex data!", (uint32_t) NewRayObject.size(), (uint32_t) sizeof(RayVertex));
                return false;
            }

            m_RayMeshBuffers.push_back(pNewBuffer);
            m_RayMeshVkBuffers.push_back(pNewBuffer->GetVkBuffer());
        }   // Vertices context

        // ****************************
        // Indices
        // ****************************
        {
            // std::vector < RayObjectIndices>     m_RayIndicesArray;
            // std::vector <BufferVulkan*>        m_RayIndiceBuffers;
            // std::vector <VkBuffer>              m_RayIndiceVkBuffers;
            RayObjectIndices NewObjectIndices;
            if (OneMesh.mesh.m_IndexBuffer.index() == 1)
            {
                // 32-Bit Values
                const auto& DataAsVec32 = std::get<std::vector<uint32_t>>(OneMesh.mesh.m_IndexBuffer);
                for (const auto& OneEntry : DataAsVec32)
                {
                    NewObjectIndices.push_back(OneEntry);
                }
            }
            else if (OneMesh.mesh.m_IndexBuffer.index() == 2)
            {
                // 16-Bit Values
                const auto& DataAsVec16 = std::get<std::vector<uint16_t>>(OneMesh.mesh.m_IndexBuffer);
                for (const auto& OneEntry : DataAsVec16)
                {
                    NewObjectIndices.push_back(OneEntry);
                }
            }
            else
            {
                LOGE("Error! Mesh index buffer has unknown type!");
                return false;
            }

            m_RayIndicesArray.push_back(NewObjectIndices);

            // Create the buffer for this object
            BufferVulkan* pNewBuffer = new BufferVulkan;
            BufferUsageFlags Usage = BufferUsageFlags::Storage | BufferUsageFlags::ShaderDeviceAddress;
            if (!pNewBuffer || !pNewBuffer->Initialize(&GetVulkan()->GetMemoryManager(), NewObjectIndices.size() * sizeof(uint32_t), Usage, NewObjectIndices.data()))
            {
                LOGE("Error! Unable to allocate %d * %d bytes for ray shader vertex data!", (uint32_t) NewObjectIndices.size(), (uint32_t) sizeof(uint32_t));
                return false;
            }

            m_RayIndiceBuffers.push_back(pNewBuffer);
            m_RayIndiceVkBuffers.push_back(pNewBuffer->GetVkBuffer());
        }   // Indices context
    }

    auto sceneRayTracable = std::make_unique<SceneRTCullable>(*GetVulkan(), m_vulkanRT);
    auto sceneRayTracableCulled = std::make_unique<SceneRTCulled>(*GetVulkan(), m_vulkanRT);

    uint32_t WhichMesh = 0;
    uint32_t WhichInstance = 0;
    for (const auto& instancedMesh : instancedMeshes)
    {
        if( !instancedMesh.mesh.m_Materials.empty() &&
            (instancedMesh.mesh.m_Materials[0].transparent || !instancedMesh.mesh.m_Materials[0].emissiveFilename.empty()) )
            continue;

        MeshObjectRT meshRayTracable;
        if (!meshRayTracable.Create(*GetVulkan(), m_vulkanRT, instancedMesh.mesh))
            return false;
        const auto& id = sceneRayTracable->AddObject(std::move(meshRayTracable));
        if (!id)
            return false;

        if (instancedMesh.mesh.m_Materials.size() != 1)
        {
            LOGE("Error! Instanced mesh does NOT have a single material!");
            return false;
        }

        WhichInstance = 0;
        for (const auto& instance : instancedMesh.instances)
        {
            sceneRayTracable->AddInstance(id, instance.transform);

            SceneDataInternal OneEntry;
            OneEntry.MeshIndex = WhichMesh;

            // The instance transform is a mat3x4, row major.
            // In other words, 3 rows of 4 entrys that look like the first 
            // 3 rows of a normal transformation matrix, with is column major.
            // Therefore, we need to create a column major mat4 from row major mat3x4
            OneEntry.Transform = glm::mat4(1.0f);
            for (uint32_t WhichCol = 0; WhichCol < 4; WhichCol++)
            {
                OneEntry.Transform[WhichCol][0] = instance.transform[0][WhichCol];
                OneEntry.Transform[WhichCol][1] = instance.transform[1][WhichCol];
                OneEntry.Transform[WhichCol][2] = instance.transform[2][WhichCol];
            }

            // The transform used for normals (Inverse Transpose)
            // See http://www.pbr-book.org/3ed-2018/Geometry_and_Transformations/Applying_Transformations.html#Normals
            // glm inverseTranspose() doesn't exist, even though in documentation :(
            OneEntry.TransformIT = glm::transpose(glm::inverse(OneEntry.Transform));

            // Set up the material for this entry (Textures are not loaded yet.  Save texture name in internal structure)
            OneEntry.DiffuseTex = instancedMesh.mesh.m_Materials[0].diffuseFilename;
            OneEntry.NormalTex = instancedMesh.mesh.m_Materials[0].bumpFilename;

            m_SceneDataInternalArray.push_back(OneEntry);

            WhichInstance++;

        }   // WhichInstance

        WhichMesh++;
    }   // WhichMesh


    // Create the acceleration structure for Ray Tracing.
    if( !sceneRayTracableCulled->CreateAccelerationStructure( MeshObjectRT::UpdateMode::InPlace, sceneRayTracable->GetNumKnownInstances() ) )
    {
        LOGE( "Error Creating Scene AccelerationStructure..." );
        return false;
    }

    m_sceneRayTracable = std::move(sceneRayTracable);
    m_sceneRayTracableCulled = std::move(sceneRayTracableCulled);

    return true;
}

//-----------------------------------------------------------------------------
void Application::Destroy()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    pVulkan->QueueWaitIdle();

    // Threads
    m_GameThreadWorker.FinishAllWork();
    m_GameThreadWorker.Terminate();

    // Command buffers
    for (auto& bufferArray : m_PassCmdBuffer)
        for (auto& buffer : bufferArray)
            buffer.Release();

    // Meshes
    m_SkyboxDrawable.reset();
    m_LightDrawable.reset();
    m_BlitDrawable.reset();
    m_SceneDrawables.clear();
    m_sceneRayTracableCulled.reset();
    m_sceneRayTracable.reset();

    // Shaders
    m_ShaderManager.reset();

    // Scene

    // Uniform Buffers
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
        {
            ReleaseUniformBuffer(pVulkan, &m_ObjectVertUniform[WhichPass][WhichBuffer]);
            ReleaseUniformBuffer(pVulkan, &m_SphereFragUniform[WhichPass][WhichBuffer]);
            ReleaseUniformBuffer(pVulkan, &m_FloorFragUniform[WhichPass][WhichBuffer]);

            ReleaseUniformBuffer(pVulkan, &m_SkyboxVertUniform[WhichPass][WhichBuffer]);
        }   // WhichBuffer
    }   // WhichPass

    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        ReleaseUniformBuffer(pVulkan, &m_BlitFragUniform[WhichBuffer]);
        ReleaseUniformBuffer(pVulkan, &m_LightFragUniform[WhichBuffer]);
    }   // WhichBuffer

    ReleaseUniformBuffer(pVulkan, &m_SceneDataUniform);
    
    // Finally call into base class destroy
    FrameworkApplicationBase::Destroy();
}

//-----------------------------------------------------------------------------
void Application::RunGameThread(const GameThreadInputParam& rThreadParam, GameThreadOutputParam& rThreadOutput)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    auto* const pVulkan = GetVulkan();
    uint32_t WhichFrame = rThreadParam.CurrentBuffer.idx;

    // Clean up command buffers from last run
    m_RenderPassSubmitData[WhichFrame].clear();

    // If anything changes with the lights (Position, orientation, color, brightness, etc.)
    UpdateLighting(rThreadParam.ElapsedTime);

    // Update uniform buffers with latest data
    UpdateUniforms(WhichFrame);

    // First pass, waits for the back buffer to be ready.
    std::span<const VkSemaphore> pWaitSemaphores = { &rThreadParam.CurrentBuffer.semaphore, 1 };

    static const VkPipelineStageFlags DefaultGfxWaitDstStageMasks[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    // **********************
    // RP_GBUFFER
    // **********************
    {
        // GBuffer render pass (gbuffer 'laydown' and shadow generation).
        auto& renderPassLocal = BeginRenderPass( RP_GBUFFER, WhichFrame, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_GBUFFER],1 } );
        AddPassCommandBuffers( renderPassLocal );
        EndRenderPass( renderPassLocal );

        pWaitSemaphores = { &m_PassCompleteSemaphore[RP_GBUFFER],1 };
    }

    // **********************
    // RP_RAYTRACE
    // **********************
    if (true)
    {
        auto& renderPassLocal = BeginRenderPass(RP_RAYTRACE, WhichFrame, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_RAYTRACE],1 });

        // Build the shadow acceleration structures (if requested to).
        m_sceneRayTracableCulled->UpdateAS( renderPassLocal.CmdBuff.m_VkCommandBuffer );

        EndRenderPass( renderPassLocal );

        pWaitSemaphores = {&m_PassCompleteSemaphore[RP_RAYTRACE], 1};
    }

    // **********************
    // RP_LIGHT
    // **********************
    {
        // Lighting pass (Gbuffer resolve and emissives)
        auto& renderPassLocal = BeginRenderPass(RP_LIGHT, WhichFrame, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_LIGHT],1 });
        AddPassCommandBuffers( renderPassLocal );
        EndRenderPass( renderPassLocal );

        pWaitSemaphores = { &m_PassCompleteSemaphore[RP_LIGHT], 1 };
    }

    // **********************
    // RP_HUD
    // **********************
    VkCommandBuffer guiCommandBuffer = VK_NULL_HANDLE;
    if (gRenderHud && m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        guiCommandBuffer = GetGui()->Render(WhichFrame, m_HudRT[0].m_FrameBuffer);
        if (guiCommandBuffer != VK_NULL_HANDLE)
        {
            auto& renderPassLocal = BeginRenderPass(RP_HUD, WhichFrame, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_HUD],1 });
            AddPassCommandBuffers( renderPassLocal, { &guiCommandBuffer,1 } );
            EndRenderPass( renderPassLocal );

            pWaitSemaphores = { &m_PassCompleteSemaphore[RP_HUD], 1 };
        }
    }

    // **********************
    // RP_BLIT
    // **********************
    // Blit Results to the screen
    {
        auto& renderPassLocal = BeginRenderPass(RP_BLIT, WhichFrame, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_PassCompleteSemaphore[RP_BLIT],1 });
        AddPassCommandBuffers( renderPassLocal );
        EndRenderPass( renderPassLocal );
        pWaitSemaphores = { &m_PassCompleteSemaphore[RP_BLIT], 1 };
    }

    rThreadOutput.Fence = rThreadParam.CurrentBuffer.fence;
    rThreadOutput.SwapchainPresentIndx = rThreadParam.CurrentBuffer.swapchainPresentIdx;
    rThreadOutput.WhichFrame = WhichFrame;
}

//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    // Reset Metrics
    m_TotalDrawCalls = 0;
    m_TotalTriangles = 0;

    // Grab the vulkan wrapper
    auto* const pVulkan = GetVulkan();

    // See if we want to change backbuffer surface format
    if (m_RequestedSurfaceFormat.colorSpace != pVulkan->m_SurfaceColorSpace || m_RequestedSurfaceFormat.format != pVulkan->m_SurfaceFormat)
    {
        ChangeSurfaceFormat(m_RequestedSurfaceFormat);
    }

    // Obtain the next swap chain image for the next frame.
    auto CurrentVulkanBuffer = pVulkan->SetNextBackBuffer();

    // ********************************
    // Application Draw() - Begin
    // ********************************

    UpdateCamera(fltDiffTime);

    // Wait for game thread to finish...
    m_GameThreadWorker.FinishAllWork();

    //
    // We are running single-threaded here...
    // 
    UpdateGui();

    if (true || gFramesToRender == 0)
    {
        //
        // Runing 'forever'.
        //
        // Do rendering 'threaded'.
        //

        // ... grab info from the last run ...
        int FrameToSubmit = m_CompletedThreadOutput.WhichFrame;
        int SwapchainPresentIndx = m_CompletedThreadOutput.SwapchainPresentIndx;
        VkFence FrameToRenderFence = m_CompletedThreadOutput.Fence;

        // Start thread updating the next frame.
        GameThreadInputParam gameThreadParam{ CurrentVulkanBuffer, fltDiffTime };

        m_GameThreadWorker.DoWork2( [](Application* pThis, Application::GameThreadInputParam ThreadParam) {
            pThis->RunGameThread(ThreadParam, pThis->m_CompletedThreadOutput);
        }, this, std::move(gameThreadParam));

        // Submit and present thread work from last frame (potentially while the thread worker is generating the next frame).
        if (FrameToSubmit >= 0)
        {
            auto finishedSemaphore = SubmitGameThreadWork(FrameToSubmit, FrameToRenderFence);
            PresentQueue(finishedSemaphore, SwapchainPresentIndx);
        }
    }
    else
    {
        //
        // Running a (pre)defined number of frames.
        //
        // Run syncronously so that we render the exact number of requested frames.
        //
        GameThreadInputParam gameThreadParam{ CurrentVulkanBuffer, fltDiffTime };
        RunGameThread(gameThreadParam, m_CompletedThreadOutput);

        // Submit and present this frame
        std::span<VkSemaphore> finishedSemaphore = SubmitGameThreadWork(m_CompletedThreadOutput.WhichFrame, m_CompletedThreadOutput.Fence);
        PresentQueue(finishedSemaphore, m_CompletedThreadOutput.SwapchainPresentIndx);
    }

    // ********************************
    // Application Draw() - End
    // ********************************
}

//-----------------------------------------------------------------------------
void Application::KeyDownEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::KeyDownEvent(key);

    uint32_t TimeNow = OS_GetTimeMS();

#ifdef OS_WINDOWS
    switch (key)
    {
    case VK_SPACE:
        // Only set this if enough time goes by
        if (guiSpacePressedThisFrame == 0 && TimeNow - guiSpacePressedTime > KEY_THIS_FRAME_TIME)
        {
            // Newly Pressed this frame
            guiSpacePressedThisFrame = 1;
            guiSpacePressedTime = OS_GetTimeMS();
        }
        break;

    case VK_SHIFT:
        guiShiftPressed = 1;
        break;

    case VK_UP:
    case VK_NUMPAD8:
        guiUpPressed = 1;
        break;

    case VK_DOWN:
    case VK_NUMPAD2:
        guiDownPressed = 1;
        break;

    case VK_LEFT:
    case VK_NUMPAD4:
        guiLeftPressed = 1;
        break;

    case VK_RIGHT:
    case VK_NUMPAD6:
        guiRightPressed = 1;
        break;

    case 'A':    guiAPressed = 1;    break;
    case 'S':    guiSPressed = 1;    break;
    case 'W':    guiWPressed = 1;    break;
    case 'D':    guiDPressed = 1;    break;
    case 'Q':    guiQPressed = 1;    break;
    case 'E':    guiEPressed = 1;    break;
    }
#endif // OS_WINDOWS
}

//-----------------------------------------------------------------------------
void Application::KeyRepeatEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::KeyRepeatEvent(key);
}

//-----------------------------------------------------------------------------
void Application::KeyUpEvent(uint32_t key)
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::KeyUpEvent(key);

    uint32_t TimeNow = OS_GetTimeMS();

#ifdef OS_WINDOWS
    switch (key)
    {
    case VK_SPACE:
        // Key is up so reset flag
        guiSpacePressedThisFrame = 0;
        break;

    case VK_SHIFT:
        guiShiftPressed = 0;
        break;

    case VK_UP:
    case VK_NUMPAD8:
        guiUpPressed = 0;
        break;

    case VK_DOWN:
    case VK_NUMPAD2:
        guiDownPressed = 0;
        break;

    case VK_LEFT:
    case VK_NUMPAD4:
        guiLeftPressed = 0;
        break;

    case VK_RIGHT:
    case VK_NUMPAD6:
        guiRightPressed = 0;
        break;

    case 'A':    guiAPressed = 0;    break;
    case 'S':    guiSPressed = 0;    break;
    case 'W':    guiWPressed = 0;    break;
    case 'D':    guiDPressed = 0;    break;
    case 'Q':    guiQPressed = 0;    break;
    case 'E':    guiEPressed = 0;    break;

    }
#endif // OS_WINDOWS

}

//-----------------------------------------------------------------------------
void Application::TouchDownEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::TouchDownEvent(iPointerID, xPos, yPos);

    m_LightChangePaused = !m_LightChangePaused;

    m_SphereCamera.TouchDownEvent(iPointerID, xPos, yPos);

    // if (gLightColorA[0] == 1.0f)
    // {
    //     gLightColorA = glm::vec4(0.1f, 0.1f, 0.1f, 2.25f);
    //     gLightColorB = glm::vec4(0.1f, 0.1f, 0.1f, 2.25f);
    // }
    // else
    // {
    //     gLightColorA = glm::vec4(1.0f, 1.0f, 1.0f, 2.25f);
    //     gLightColorB = glm::vec4(1.0f, 1.0f, 1.0f, 2.25f);
    // }
}

//-----------------------------------------------------------------------------
void Application::TouchMoveEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::TouchMoveEvent(iPointerID, xPos, yPos);

    m_SphereCamera.TouchMoveEvent(iPointerID, xPos, yPos);
}

//-----------------------------------------------------------------------------
void Application::TouchUpEvent(int iPointerID, float xPos, float yPos)
//-----------------------------------------------------------------------------
{
    ApplicationHelperBase::TouchUpEvent(iPointerID, xPos, yPos);

    m_SphereCamera.TouchUpEvent(iPointerID, xPos, yPos);
}


//-----------------------------------------------------------------------------
bool Application::LoadTextures()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    // Load 'loose' textures
    m_TextureManager->GetOrLoadTexture("Environment", *m_AssetManager, "./Media/Textures/simplesky_env.ktx", SamplerAddressMode::Repeat);
    m_TextureManager->GetOrLoadTexture("Irradiance", *m_AssetManager, "./Media/Textures/simplesky_irradiance.ktx", SamplerAddressMode::Repeat);

    m_TexWhite = LoadKTXTexture(GetVulkan(), *m_AssetManager, "./Media/Textures/white_d.ktx", SamplerAddressMode::Repeat);
    m_DefaultNormal = LoadKTXTexture(GetVulkan(), *m_AssetManager, "./Media/Textures/normal_default.ktx", SamplerAddressMode::Repeat);

    if (m_TexWhite.IsEmpty() || m_DefaultNormal.IsEmpty())
    {
        return false;
    }

    // m_TexBunnySplash = LoadKTXTexture(GetVulkan(), *m_AssetManager, "./Media/Textures/bbb_splash.ktx", SamplerAddressMode::ClampEdge);
    // m_loadedTextures.emplace("Bunny", *m_AssetManager, "./Media/Textures/bbb_splash.ktx", SamplerAddressMode::ClampEdge);

    m_TextureManager->GetOrLoadTexture("bbb_splash", *m_AssetManager, "./Media/Textures/stub_checker.ktx", SamplerAddressMode::ClampEdge);
    m_TextureManager->GetOrLoadTexture("stub_red", *m_AssetManager, "./Media/Textures/stub_red.ktx", SamplerAddressMode::Repeat);
    m_TextureManager->GetOrLoadTexture("stub_green", *m_AssetManager, "./Media/Textures/stub_green.ktx", SamplerAddressMode::Repeat);
    m_TextureManager->GetOrLoadTexture("stub_blue", *m_AssetManager, "./Media/Textures/stub_blue.ktx", SamplerAddressMode::Repeat);
    m_TextureManager->GetOrLoadTexture("stub_pink", *m_AssetManager, "./Media/Textures/stub_pink.ktx", SamplerAddressMode::Repeat);
    m_TextureManager->GetOrLoadTexture("stub_black", *m_AssetManager, "./Media/Textures/stub_black.ktx", SamplerAddressMode::Repeat);
    m_TextureManager->GetOrLoadTexture("stub_white", *m_AssetManager, "./Media/Textures/stub_white.ktx", SamplerAddressMode::Repeat);
    m_TextureManager->GetOrLoadTexture("stub_checker", *m_AssetManager, "./Media/Textures/stub_checker.ktx", SamplerAddressMode::Repeat);
    m_TextureManager->GetOrLoadTexture("stub_normal", *m_AssetManager, "./Media/Textures/normal_default.ktx", SamplerAddressMode::Repeat);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::CreateRayInputs()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    // Need the textures in an array that can be passed to shader
    m_SceneTextures.clear();

    // Lookup for m_SceneTextures while we fill it with texture pointers.
    std::map<std::string, size_t> sceneTextureIndices;

    auto getLoadedTextureIndex = [this, &sceneTextureIndices]( const std::string& name ) -> size_t {
        auto [it, emplaced] = sceneTextureIndices.try_emplace( name, 0 );
        if (emplaced)
        {
            const Texture* pTexture = m_TextureManager->GetTexture( name );
            assert( pTexture );    // We expect to find all the textures we are looking for!
            size_t index = &m_SceneTextures.emplace_back( pTexture ) - m_SceneTextures.data();
            it->second = index;
        }
        return it->second;
    };

    // After textures are loaded we can fill out scene data.
    // Loop through the internal structure and fill in the blanks

    for (auto& OneInternal : m_SceneDataInternalArray)
    {
        SceneData NewSceneData;
        NewSceneData.MeshIndex = OneInternal.MeshIndex;

        NewSceneData.Transform = OneInternal.Transform;
        NewSceneData.TransformIT = OneInternal.TransformIT;


        NewSceneData.DiffuseTex = getLoadedTextureIndex(OneInternal.DiffuseTex);
        NewSceneData.NormalTex = getLoadedTextureIndex(OneInternal.NormalTex);

        m_SceneDataArray.push_back(NewSceneData);
    }

    // Make the actual scene data buffer (vkBuffer to pass to material)
    uint32_t dataSize = (uint32_t)(sizeof(SceneData) * m_SceneDataArray.size());
    BufferUsageFlags usage = BufferUsageFlags::Storage | BufferUsageFlags::Uniform;
    if (!CreateUniformBuffer(pVulkan, &m_SceneDataUniform, dataSize, m_SceneDataArray.data(), usage))
    {
        LOGE("Unable to create uniform buffer for scene data!");
        assert(0);
        return false;
    }

    return true;
}

bool Application::CreateRenderTargets()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Creating Render Targets...");
    LOGI("******************************");

    auto* const pVulkan = GetVulkan();

    const TextureFormat DesiredDepthFormat = pVulkan->GetBestSurfaceDepthFormat();

    // Setup the GBuffer
    const TextureFormat GbufferColorType[] = { 
        TextureFormat::R8G8B8A8_UNORM,           // Albedo
        TextureFormat::R16G16B16A16_SFLOAT,      // Normals
        TextureFormat::R8G8B8A8_UNORM,           // Material Properties
    };

    if (!m_GBufferRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, GbufferColorType, DesiredDepthFormat, VK_SAMPLE_COUNT_1_BIT, "GBuffer RT"))
    {
        LOGE("Unable to create gbuffer render target");
    }

    // Setup the 'main' (compositing) buffer
    const TextureFormat MainColorType[] = { TextureFormat::R16G16B16A16_SFLOAT };

    if (!m_MainRT.Initialize(pVulkan, gRenderWidth, gRenderHeight, MainColorType, m_GBufferRT/*inherit depth*/, VK_SAMPLE_COUNT_1_BIT, "Main RT"))
    {
        LOGE("Unable to create main render target");
    }

    // Setup the HUD render target (no depth)
    const TextureFormat HudColorType[] = { TextureFormat::R8G8B8A8_UNORM };

    // Surface Width/Height can be 0 and on Android it is set to surface width
    // On Windows is is still left at 0.
    uint32_t HudWidth = gSurfaceWidth;
    if(HudWidth == 0)
        HudWidth = gRenderWidth;

    uint32_t HudHeight = gSurfaceHeight;
    if (HudHeight == 0)
        HudHeight = gRenderHeight;

    if (!m_HudRT.Initialize(pVulkan, HudWidth, HudHeight, HudColorType, TextureFormat::UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "HUD RT"))
    {
        LOGE("Unable to create hud render target");
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitShadowMap()
//-----------------------------------------------------------------------------
{
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitLighting()
//-----------------------------------------------------------------------------
{
    m_LightColor = { 1.0f, 0.98f, 0.73f, 50.0f };

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadConstantShaders()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    m_ShaderManager->RegisterRenderPassNames({ sRenderPassNames, sRenderPassNames + (sizeof(sRenderPassNames) / sizeof(sRenderPassNames[0])) });

    LOGI("Loading Shaders...");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
        { tIdAndFilename { "ObjectDeferred", "Media\\Shaders\\ObjectDeferred.json" },
          tIdAndFilename { "Skybox", "Media\\Shaders\\Skybox.json" },
          // tIdAndFilename { "Light", "Media\\Shaders\\Light.json" },
          tIdAndFilename { "Blit", "Media\\Shaders\\Blit.json" }
        })
    {
        LOGI("    %s", i.second.c_str());
        if (!m_ShaderManager->AddShader(*m_AssetManager, i.first, i.second))
        {
            LOGE("Error Loading shader %s from %s", i.first.c_str(), i.second.c_str());
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadDynamicShaders()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    LOGI("Loading Shaders (With dynamic elements)...");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
        { 
        tIdAndFilename { "Light", "Media\\Shaders\\Light.json" },
        })
    {
        LOGI("    %s", i.second.c_str());
        if (!m_ShaderManager->AddShader(*m_AssetManager, i.first, i.second))
        {
            LOGE("Error Loading shader %s from %s", i.first.c_str(), i.second.c_str());
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadDynamicRayTracingObjects()
//-----------------------------------------------------------------------------
{
    if (true)    // Light
    {
        //
        // Create the 'Light' drawable mesh.
        // Fullscreen quad that does the gbuffer lighting pass (fullscreen pass of gbuffer, outputting a lit color buffer).
        LOGI("Creating Light mesh...");
        MeshObject lightMesh;
        MeshHelper::CreateScreenSpaceMesh(GetVulkan()->GetMemoryManager(), 0, &lightMesh);

        const auto* pLightShader = m_ShaderManager->GetShader("Light");
        assert(pLightShader);
        auto lightShaderMaterial = m_MaterialManager->CreateMaterial(*GetVulkan(), *pLightShader, NUM_VULKAN_BUFFERS,
            [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo {
            if (texName == "TexArray") {
                return m_SceneTextures;
            }
            else if (texName == "Albedo") {
                return { &m_GBufferRT[0].m_ColorAttachments[0] };
            }
            else if (texName == "Normal") {
                return { &m_GBufferRT[0].m_ColorAttachments[1] };
            }
            else if (texName == "MatProps") {
                return { &m_GBufferRT[0].m_ColorAttachments[2] };
            }
            else if (texName == "Depth") {
                return { &m_GBufferRT[0].m_DepthAttachment };
            }
            else if (texName == "Environment")
            {
                auto* pTexture = m_TextureManager->GetTexture(texName);
                if (pTexture)
                    return { pTexture };

                assert(0);
                return {};
            }
            else if (texName == "Irradiance")
            {
                auto* pTexture = m_TextureManager->GetTexture(texName);
                if (pTexture)
                    return { pTexture };

                assert(0);
                return {};
            }
            else
            {
                LOGE("Texture not handled in Light material: %s", texName.c_str());
                assert(0);
                return {};
            }
        },
            [this](const std::string& bufferName) -> tPerFrameVkBuffer {
                if (bufferName == "FragUB")
                {
                    // This same uniform is used for both RT and Irradiance
                    return { m_LightFragUniform[0].buf.GetVkBuffer() };
                }
                else if (bufferName == "SceneData")
                {
                    return { m_SceneDataUniform.buf.GetVkBuffer() };
                }
                else if (bufferName == "VertData")
                {
                    return m_RayMeshVkBuffers;
                }
                else if (bufferName == "IndiceData")
                {
                    return m_RayIndiceVkBuffers;
                }
                else
                {
                    LOGE("Buffer not handled in Light material: %s", bufferName.c_str());
                    assert(0);
                    return {};
                }
        },
            nullptr,
            [this](const std::string& accelStructureName) -> MaterialPass::tPerFrameVkAccelerationStructure {
            return { m_sceneRayTracableCulled->GetVkAccelerationStructure() };
        }
        );

        m_LightDrawable = std::make_unique<Drawable>(*GetVulkan(), std::move(lightShaderMaterial));
        if (!m_LightDrawable->Init(m_RenderPass[RP_LIGHT], sRenderPassNames[RP_LIGHT], std::move(lightMesh)))
        {
            LOGE("Error Creating Light drawable...");
        }

    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitUniforms()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    // These are only created here, they are not set to initial values
    LOGI("Creating uniform buffers...");

    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
        {
            CreateUniformBuffer(pVulkan, m_ObjectVertUniform[WhichPass][WhichBuffer]);
            CreateUniformBuffer(pVulkan, m_SphereFragUniform[WhichPass][WhichBuffer]);
            CreateUniformBuffer(pVulkan, m_FloorFragUniform[WhichPass][WhichBuffer]);

            CreateUniformBuffer(pVulkan, m_SkyboxVertUniform[WhichPass][WhichBuffer]);
        }   // WhichBuffer
    }   // WhichPass

    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        CreateUniformBuffer(pVulkan, m_BlitFragUniform[WhichBuffer]);
        CreateUniformBuffer(pVulkan, m_LightFragUniform[WhichBuffer]);
    }

    return true;
}

//-----------------------------------------------------------------------------
glm::mat4 Application::CreateRandomRotation()
//-----------------------------------------------------------------------------
{
// #ifdef TEST_METHOD
    // Rotate randomly around all three axis
    glm::mat4 RetVal = glm::identity<glm::mat4>();

    float OneRotation = PI_MUL_2 * (float)rand() / (float)RAND_MAX;
    RetVal = glm::rotate(RetVal, OneRotation, glm::vec3(1.0f, 0.0f, 0.0f));

    OneRotation = PI_MUL_2 * (float)rand() / (float)RAND_MAX;
    RetVal = glm::rotate(RetVal, OneRotation, glm::vec3(0.0f, 1.0f, 0.0f));

    OneRotation = PI_MUL_2 * (float)rand() / (float)RAND_MAX;
    RetVal = glm::rotate(RetVal, OneRotation, glm::vec3(0.0f, 0.0f, 1.0f));

    // #error LOGE("Probe Ray Transform NOT random!");
    // RetVal = glm::identity<glm::mat4>();

    return RetVal;
// #endif // TEST_METHOD

#ifdef LONG_METHOD
    // #error Create three random rotations and multiply

    glm::mat4 RetVal = glm::identity<glm::mat4>();

    // Random around the pole in [0, 1]
    float ThetaVal = PI_MUL_2 * (float)rand() / (float)RAND_MAX;
    float cosTheta = std::cosf(ThetaVal);
    float sinTheta = std::sinf(ThetaVal);

    // Random direction to deflect pole in [0, 1]
    float PhiVal = PI_MUL_2 * (float)rand() / (float)RAND_MAX;
    float cosPhi = std::cosf(PhiVal);
    float sinPhi = std::sinf(PhiVal);

    // Random amount of pole deflection
    float ZVal = (float)rand() / (float)RAND_MAX;

    float sq3 = 2.f * std::sqrtf(ZVal * (1.f - ZVal));

    float s2 = 2.f * ZVal * sinTheta * sinTheta - 1.f;
    float c2 = 2.f * ZVal * cosTheta * cosTheta - 1.f;
    float sc = 2.f * ZVal * sinTheta * cosTheta;

    // Create the random rotation matrix
    float _11 = cosTheta * c2 - sinTheta * sc;
    float _12 = sinTheta * c2 + cosTheta * sc;
    float _13 = sq3 * cosTheta;

    float _21 = cosTheta * sc - sinTheta * s2;
    float _22 = sinTheta * sc + cosTheta * s2;
    float _23 = sq3 * sinTheta;

    float _31 = cosTheta * (sq3 * cosTheta) - sinTheta * (sq3 * sinTheta);
    float _32 = sinTheta * (sq3 * cosTheta) + cosTheta * (sq3 * sinTheta);
    float _33 = 1.f - 2.f * ZVal;

    // Set columns to output
    RetVal[0] = glm::vec4(_11, _12, _13, 0.0f);
    RetVal[1] = glm::vec4(_21, _22, _23, 0.0f);
    RetVal[2] = glm::vec4(_31, _32, _33, 0.0f);
    RetVal[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    // #error LOGE("Probe Ray Transform NOT random!");
    // RetVal = glm::identity<glm::mat4>();

    return RetVal;
#endif // LONG_METHOD

}

//-----------------------------------------------------------------------------
bool Application::UpdateUniforms(uint32_t WhichBuffer)
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    // Things that change over time
    if(m_LastUpdateTime == 0)
        m_LastUpdateTime = OS_GetTimeMS();

    uint32_t TimeNow = OS_GetTimeMS();
    uint32_t TimeDiff = TimeNow - m_LastUpdateTime;
    m_LastUpdateTime = TimeNow;

#ifdef OS_WINDOWS
    if (guiSpacePressedThisFrame == 1)
    {
        m_LightChangePaused = !m_LightChangePaused;
    }
#endif // OS_WINDOWS


    // For debugging, want to control how big a time jump we can get
    if (TimeDiff > 100)
        TimeDiff = 100;

    if(!m_LightChangePaused)
        m_LightDirUpdateTime += TimeDiff;
    if (gLightDirChangeRate != 0.0f && m_LightDirUpdateTime > (uint32_t)(gLightDirChangeRate * 1000.0f))
    {
        // Hit the end.  Pull off "extra" and reverse direction.
        m_LightDirUpdateTime -= (uint32_t)(gLightDirChangeRate * 1000.0f);
        m_LightDirReverse = !m_LightDirReverse;
    }

    if (!m_LightChangePaused)
        m_LightColorUpdateTime += TimeDiff;
    if (gLightColorChangeRate != 0.0f && m_LightColorUpdateTime > (uint32_t)(gLightColorChangeRate * 1000.0f))
    {
        // Hit the end.  Pull off "extra" and reverse direction.
        m_LightColorUpdateTime -= (uint32_t)(gLightColorChangeRate * 1000.0f);
        m_LightColorReverse = !m_LightColorReverse;
    }

    // Want light direction to be normalized
    glm::vec3 LocalLightDir;
    float LightDirLerp = 1.0f;
    if(gLightDirChangeRate != 0.0)
        LightDirLerp = (float)m_LightDirUpdateTime / (gLightDirChangeRate * 1000.0f);
    if(m_LightDirReverse)
        LocalLightDir = L_Lerp(LightDirLerp, gLightDirB, gLightDirA);
    else
        LocalLightDir = L_Lerp(LightDirLerp, gLightDirA, gLightDirB);

    LocalLightDir = glm::normalize(LocalLightDir);

    glm::vec4 LocalLightColor;
    float LightColorLerp = 1.0f;
    if(gLightColorChangeRate != 0.0f)
        LightColorLerp = (float)m_LightColorUpdateTime / (gLightColorChangeRate * 1000.0f);
    if(m_LightColorReverse)
        LocalLightColor = L_Lerp(LightColorLerp, gLightColorB, gLightColorA);
    else
        LocalLightColor = L_Lerp(LightColorLerp, gLightColorA, gLightColorB);


    // Special View matrix for Skybox
    glm::mat4 SkyboxViewMatrix;

    // ********************************
    // Object
    // ********************************
    glm::mat4 ObjectVP = m_SphereCamera.ProjectionMatrix() * m_SphereCamera.ViewMatrix();

    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        // No uniform buffers for HUD or BLIT since objects not in HUD and BLIT
        if (WhichPass == RP_HUD || WhichPass == RP_BLIT || WhichPass == RP_LIGHT || WhichPass == RP_RAYTRACE)
            continue;

        switch (WhichPass)
        {
        case RP_GBUFFER:
            m_ObjectVertUniformData.VPMatrix = ObjectVP;
            break;
        default:
            LOGE("***** %s Not Handled! *****", GetPassName(WhichPass));
            break;
        }

        UpdateUniformBuffer(pVulkan, m_ObjectVertUniform[WhichPass][WhichBuffer], m_ObjectVertUniformData);

        m_ObjectFragUniformData.Color = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);  // White by default
        m_ObjectFragUniformData.MaterialProps = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);  // Max Ray Bounces (Really up to 255).  Minimized by config variable
        UpdateUniformBuffer(pVulkan, m_SphereFragUniform[WhichPass][WhichBuffer], m_ObjectFragUniformData);

        m_ObjectFragUniformData.Color = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);  // White by default
        m_ObjectFragUniformData.MaterialProps = glm::vec4(gFloorRayBounces / 255.0f, 0.0f, 0.0f, 0.0f);  // Config Ray Bounces
        UpdateUniformBuffer(pVulkan, m_FloorFragUniform[WhichPass][WhichBuffer], m_ObjectFragUniformData);
    }

    // ********************************
    // Skybox
    // ********************************
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        // No uniform buffers for HUD or BLIT since objects not in HUD and BLIT
        if (WhichPass == RP_HUD || WhichPass == RP_BLIT)
            continue;

        glm::mat4 LocalModel = glm::scale(glm::vec3(m_SkyboxScale, m_SkyboxScale, m_SkyboxScale));

        // Special View matrix for Skybox (always centered on the view position)
        SkyboxViewMatrix = m_SphereCamera.ViewMatrix();
        // m_SphereCamera.GetViewMatrix(SkyboxViewMatrix);
        SkyboxViewMatrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::mat4 SkyboxMVP = m_SphereCamera.ProjectionMatrix() * SkyboxViewMatrix * LocalModel;

        m_SkyboxVertUniformData.MVPMatrix = SkyboxMVP;
        m_SkyboxVertUniformData.ModelMatrix = LocalModel;
        m_SkyboxVertUniformData.Color = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);  // White by default
        UpdateUniformBuffer(pVulkan, m_SkyboxVertUniform[WhichPass][WhichBuffer], m_SkyboxVertUniformData);
    }

    // ********************************
    // Fullscreen Light pass
    // ********************************

    glm::mat4 CameraViewInv = glm::inverse(m_SphereCamera.ViewMatrix());
    glm::mat4 CameraProjectionInv = glm::inverse(m_SphereCamera.ProjectionMatrix());

    m_LightFragUniformData.ProjectionInv = CameraProjectionInv;
    m_LightFragUniformData.ViewInv = CameraViewInv;

    m_LightFragUniformData.ShaderTint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    m_LightFragUniformData.EyePos = glm::vec4(m_SphereCamera.Position(), 1.0f);
    m_LightFragUniformData.LightDir = glm::vec4(LocalLightDir, 1.0f);
    m_LightFragUniformData.LightColor = LocalLightColor;

    // X: Mip Level
    // Y: Reflect Amount
    // Z: Reflect Fresnel Min
    // W: Reflect Fresnel Max
    m_LightFragUniformData.ReflectParams = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);

    // X: Mip Level
    // Y: IBL Amount
    // Z: Not Used
    // W: Not Used
    m_LightFragUniformData.IrradianceParams = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f);

    // X: Ray Min Distance
    // Y: Ray Max Distance
    // Z: Max Ray Bounces
    // W: Ray Blend Amount (Mirror Amount)
    m_LightFragUniformData.RayParam01 = glm::vec4(gRayMinDistance, gRayMaxDistance, (float)gSphereRayBounces, gRayBlendAmount);

    // X: Shadow Amount
    // Y: Minumum Normal Dot Light
    // Z: Not Used
    // W: Not Used
    m_LightFragUniformData.LightParams = glm::vec4(gShadowAmount, gMinNormDotLight, 0.0f, 0.0f);

    UpdateUniformBuffer(pVulkan, m_LightFragUniform[WhichBuffer], m_LightFragUniformData);

    // ********************************
    // Blit
    // ********************************

    m_BlitFragUniformData.sRGB = m_bEncodeSRGB ? 1 : 0;
    UpdateUniformBuffer(pVulkan, m_BlitFragUniform[WhichBuffer], m_BlitFragUniformData);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::BuildCmdBuffers()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    LOGI("******************************");
    LOGI("Building Command Buffers...");
    LOGI("******************************");

    uint32_t TargetWidth = GetVulkan()->m_SurfaceWidth;
    uint32_t TargetHeight = GetVulkan()->m_SurfaceHeight;
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        // If viewport and scissor are dynamic we need to add them to the secondary buffers.
        const uint32_t TargetWidth = m_PassSetup[WhichPass].TargetWidth;
        const uint32_t TargetHeight = m_PassSetup[WhichPass].TargetHeight;

        VkViewport Viewport = {};
        Viewport.x = 0.0f;
        Viewport.y = 0.0f;
        Viewport.width = (float)TargetWidth;
        Viewport.height = (float)TargetHeight;
        Viewport.minDepth = 0.0f;
        Viewport.maxDepth = 1.0f;

        VkRect2D Scissor = {};
        Scissor.offset.x = 0;
        Scissor.offset.y = 0;
        Scissor.extent.width = TargetWidth;
        Scissor.extent.height = TargetHeight;

        for (uint32_t WhichBuffer = 0; WhichBuffer < GetVulkan()->m_SwapchainImageCount; WhichBuffer++)
        {
            // Set up some values that change based on render pass
            VkRenderPass WhichRenderPass = VK_NULL_HANDLE;
            VkFramebuffer WhichFramebuffer = pVulkan->m_SwapchainBuffers[WhichBuffer].framebuffer;
            switch (WhichPass)
            {
            case RP_GBUFFER:
                WhichRenderPass = m_GBufferRT.m_RenderPass;
                WhichFramebuffer = m_GBufferRT[0].m_FrameBuffer;
                break;

            case RP_LIGHT:
                WhichRenderPass = m_MainRT.m_RenderPass;
                WhichFramebuffer = m_MainRT[0].m_FrameBuffer;
                break;

            case RP_HUD:
                WhichRenderPass = m_HudRT.m_RenderPass;
                WhichFramebuffer = m_HudRT[0].m_FrameBuffer;
                break;

            case RP_BLIT:
                WhichRenderPass = m_RenderPass[WhichPass];
                WhichFramebuffer = pVulkan->m_SwapchainBuffers[WhichBuffer].framebuffer;
                break;
            }

            if (WhichPass == RP_LIGHT)
            {
                // Light deferred gbuffer
                if (!m_LightCmdBuffer[WhichBuffer].Begin(WhichFramebuffer, WhichRenderPass))
                {
                    return false;
                }
                vkCmdSetViewport(m_LightCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Viewport);
                vkCmdSetScissor(m_LightCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Scissor);
            }

            if (WhichPass == RP_BLIT)
            {
                // Blit (only in the blit pass)
                if (!m_BlitCmdBuffer[WhichBuffer].Begin(WhichFramebuffer, WhichRenderPass, true/*swapchain renderpass*/))
                {
                    return false;
                }
                vkCmdSetViewport(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Viewport);
                vkCmdSetScissor(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Scissor);
            }
            else
            {
                // Objects (can render into any pass except Blit)
                if (!m_ObjectCmdBuffer[WhichBuffer][WhichPass].Begin(WhichFramebuffer, WhichRenderPass))
                {
                    return false;
                }
                vkCmdSetViewport(m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_VkCommandBuffer, 0, 1, &Viewport);
                vkCmdSetScissor(m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_VkCommandBuffer, 0, 1, &Scissor);
            }
        }   // Which Buffer
    }   // Which Pass

    // Run through the drawables (each one may be in multiple command buffers)
    for (const auto& drawable : m_SceneDrawables)
    {
        AddDrawableToCmdBuffers(drawable, &m_ObjectCmdBuffer[0][0], NUM_RENDER_PASSES, GetVulkan()->m_SwapchainImageCount);
    }

    // Add the skybox (last)
    if (m_SkyboxDrawable)
    {
        AddDrawableToCmdBuffers(*m_SkyboxDrawable, &m_ObjectCmdBuffer[0][0], NUM_RENDER_PASSES, GetVulkan()->m_SwapchainImageCount);
    }

    // and end their pass
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        if (WhichPass != RP_BLIT)
        {
            for (uint32_t WhichBuffer = 0; WhichBuffer < GetVulkan()->m_SwapchainImageCount; WhichBuffer++)
            {
                if (!m_ObjectCmdBuffer[WhichBuffer][WhichPass].End())
                {
                    return false;
                }
            }
        }
    }

    // Do the light commands
    AddDrawableToCmdBuffers(*m_LightDrawable.get(), m_LightCmdBuffer, 1, GetVulkan()->m_SwapchainImageCount);
    for (uint32_t WhichBuffer = 0; WhichBuffer < GetVulkan()->m_SwapchainImageCount; WhichBuffer++)
    {
        if (!m_LightCmdBuffer[WhichBuffer].End())
        {
            return false;
        }
    }

    // Do the blit commands
    AddDrawableToCmdBuffers(*m_BlitDrawable.get(), m_BlitCmdBuffer, 1, GetVulkan()->m_SwapchainImageCount);
    for (uint32_t WhichBuffer = 0; WhichBuffer < GetVulkan()->m_SwapchainImageCount; WhichBuffer++)
    {
        if (!m_BlitCmdBuffer[WhichBuffer].End())
        {
            return false;
        }
    }

    return true;
}


//-----------------------------------------------------------------------------
bool Application::InitLocalSemaphores()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    auto* const pVulkan = GetVulkan();

    VkSemaphoreCreateInfo SemaphoreInfo = {};
    SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    SemaphoreInfo.pNext = NULL;
    SemaphoreInfo.flags = 0;

    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        RetVal = vkCreateSemaphore(pVulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_PassCompleteSemaphore[WhichPass]);
        if (!CheckVkError("vkCreateSemaphore()", RetVal))
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
const char* Application::GetPassName(uint32_t WhichPass)
//-----------------------------------------------------------------------------
{
    if (WhichPass >= sizeof(sRenderPassNames) / sizeof(sRenderPassNames[0]))
    {
        LOGE("GetPassName() called with unknown pass (%d)!", WhichPass);
        return "RP_UNKNOWN";
    }
    return sRenderPassNames[WhichPass];
}

//-----------------------------------------------------------------------------
bool Application::InitMaterials()
//-----------------------------------------------------------------------------
{
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitCommandBuffers()
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    char szName[256];
    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
        {
            // The Pass Command Buffer => Primary
            sprintf(szName, "Primary (%s; Buffer %d of %d)", GetPassName(WhichPass), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_PassCmdBuffer[WhichBuffer][WhichPass].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
            {
                return false;
            }

            // Model => Secondary
            sprintf(szName, "Model (%s; Buffer %d of %d)", GetPassName(WhichPass), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_ObjectCmdBuffer[WhichBuffer][WhichPass].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
            {
                return false;
            }

        }
    }
    // Blit => Secondary
    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        sprintf(szName, "Blit (%s; Buffer %d of %d)", GetPassName(RP_BLIT), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
        if (!m_BlitCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
        {
            return false;
        }
    }

    // Light => Secondary
    for (uint32_t WhichBuffer = 0; WhichBuffer < NUM_VULKAN_BUFFERS; WhichBuffer++)
    {
        sprintf(szName, "Light (%s; Buffer %d of %d)", GetPassName(RP_LIGHT), WhichBuffer + 1, NUM_VULKAN_BUFFERS);
        if (!m_LightCmdBuffer[WhichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
        {
            return false;
        }
    }

    return true;
}


//-----------------------------------------------------------------------------
bool Application::InitAllRenderPasses()
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    auto* const pVulkan = GetVulkan();

    // Fill in the Setup data
    m_PassSetup[RP_GBUFFER] = { m_GBufferRT[0].m_pLayerFormats, m_GBufferRT[0].m_DepthFormat,      RenderPassInputUsage::Clear,   true,  RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Store,      gClearColor,m_GBufferRT[0].m_Width,   m_GBufferRT[0].m_Height };
    m_PassSetup[RP_RAYTRACE] ={ {},                             TextureFormat::UNDEFINED,               RenderPassInputUsage::DontCare,false, RenderPassOutputUsage::Discard,       RenderPassOutputUsage::Discard,    {},          m_GBufferRT[0].m_Width,   m_GBufferRT[0].m_Height };
    m_PassSetup[RP_LIGHT] =   { m_MainRT[0].m_pLayerFormats,    m_GBufferRT[0].m_DepthFormat,      RenderPassInputUsage::Clear,   false, RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Discard,    {},          m_MainRT[0].m_Width,      m_MainRT[0].m_Height };
    m_PassSetup[RP_HUD] =     { m_HudRT[0].m_pLayerFormats,     m_HudRT[0].m_DepthFormat,          RenderPassInputUsage::Clear,   false, RenderPassOutputUsage::StoreReadOnly, RenderPassOutputUsage::Discard,    {},          m_HudRT[0].m_Width,       m_HudRT[0].m_Height };
    m_PassSetup[RP_BLIT] =    { {pVulkan->m_SurfaceFormat},     pVulkan->m_SwapchainDepth.format,  RenderPassInputUsage::DontCare,false, RenderPassOutputUsage::Present,       RenderPassOutputUsage::Discard,    {},          GetVulkan()->m_SurfaceWidth, GetVulkan()->m_SurfaceHeight };

    LOGI("******************************");
    LOGI("Initializing Render Passes... ");
    LOGI("******************************");
    for (uint32_t WhichPass = 0; WhichPass < NUM_RENDER_PASSES; WhichPass++)
    {
        const auto& PassSetup = m_PassSetup[WhichPass];
        bool IsSwapChainRenderPass = WhichPass == RP_BLIT;

        if (PassSetup.ColorFormats.size() > 0 || PassSetup.DepthFormat != TextureFormat::UNDEFINED)
        {
            if (!GetVulkan()->CreateRenderPass({PassSetup.ColorFormats},
                PassSetup.DepthFormat,
                VK_SAMPLE_COUNT_1_BIT,
                PassSetup.ColorInputUsage,
                PassSetup.ColorOutputUsage,
                PassSetup.ClearDepthRenderPass,
                PassSetup.DepthOutputUsage,
                &m_RenderPass[WhichPass]))
                return false;
        }

        // LOGI("    Render Pass (%s; Buffer %d of %d) => 0x%x", GetPassName(WhichPass), WhichBuffer + 1, NUM_VULKAN_BUFFERS, m_RenderPass[WhichPass][WhichBuffer]);
    }   // Which Pass

    return true;
}


//-----------------------------------------------------------------------------
bool Application::InitDrawables()
//-----------------------------------------------------------------------------
{
    // Part of the kludge to get callback function to return the correct buffer
    const uint32_t Mat_Sphere = 5150;
    const uint32_t Mat_Floor = 1999;

    const auto& bufferLoader = [&](const uint32_t MaterialID, const std::string& bufferSlotName) -> tPerFrameVkBuffer {
        if (bufferSlotName == "Vert")
        {
            return { m_ObjectVertUniform[RP_GBUFFER][0].buf.GetVkBuffer() };
        }
        else if (bufferSlotName == "Frag")
        {
            switch (MaterialID)
            {
            case Mat_Sphere:
                return { m_SphereFragUniform[RP_GBUFFER][0].buf.GetVkBuffer() };
            case Mat_Floor:
                return { m_FloorFragUniform[RP_GBUFFER][0].buf.GetVkBuffer() };
            default:
                assert(0);
                return {};
            }

        }
        else
        {
            assert(0);
            return {};
        }
    };

    const auto* pOpaqueShader = m_ShaderManager->GetShader("ObjectDeferred");
    if (!pOpaqueShader)
    {
        // Error (bad shaderName)
        return false;
    }

    // m_TextureManager->SetDefaultFilenameManipulators(PathManipulator_PrefixDirectory{ "Media/Textures/" }, PathManipulator_ChangeExtension{ ".ktx" });

    if (1)
    {
        auto sceneTextureLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef, const std::string& textureSlotName) -> const MaterialPass::tPerFrameTexInfo
        {

            std::string textureName;
            bool normalMap = false;

            if (textureSlotName == "Diffuse")
            {
                textureName = materialDef.diffuseFilename;
            }
            else if (textureSlotName == "Normal")
            {
                normalMap = true;
                textureName = materialDef.bumpFilename;
                if (textureName.empty())
                {
                    LOGE("No texture specified for slot (Using flat default): %s", textureSlotName.c_str());
                    return { &m_DefaultNormal };
                }
            }
            else if (textureSlotName == "SpecMap")
            {
                textureName = materialDef.specMapFilename;
                if (textureName.empty())
                {
                    LOGE("No texture specified for slot (Using white default): %s", textureSlotName.c_str());
                    return { &m_TexWhite };
                }
            }
            else if (textureSlotName == "Environment")
            {
                auto* pTexture = m_TextureManager->GetTexture(textureSlotName);
                if (pTexture)
                    return { pTexture };

                // File not loaded, use default
                LOGE("No texture loaded for slot: %s", textureSlotName.c_str());
                assert(0);
                return {};
            }
            else if (textureSlotName == "Irradiance")
            {
                auto* pTexture = m_TextureManager->GetTexture(textureSlotName);
                if (pTexture)
                    return { pTexture };

                // File not loaded, use default
                LOGE("No texture loaded for slot: %s", textureSlotName.c_str());
                assert(0);
                return {};
            }
            else
            {
                LOGE("Texture slot NOT handled: %s", textureSlotName.c_str());
                assert(0);
            }

            if (textureName.empty())
            {
                LOGE("No texture specified for slot (Using white default): %s", textureSlotName.c_str());
                // LOGE("    Diffuse: %s", materialDef.diffuseFilename.c_str());
                // LOGE("    Bump: %s", materialDef.bumpFilename.c_str());
                // LOGE("    Emissive: %s", materialDef.emissiveFilename.c_str());
                // LOGE("    SpecMap: %s", materialDef.specMapFilename.c_str());
                // LOGE("    alphaCutout: %s", materialDef.alphaCutout ? "True" : "False");
                // LOGE("    Transparent: %s", materialDef.transparent ? "True" : "False");
                return { &m_TexWhite };
            }

            auto pTexture = m_TextureManager->GetTexture(textureName);
            if (!pTexture)
            {
                LOGE("Referenced texture has NOT been loaded: %s", textureName.c_str());
                return { &m_TexWhite };
            }

            if (!pTexture)
            {
                if (normalMap)
                {
                    LOGE("NormalMap texture not found (Using flat default): %s", textureName.c_str());
                    return { &m_DefaultNormal };
                }

                // File not loaded, use default
                // #error This is incorrect.  The texture array size will now be one less than the count!  Material will crash.
                LOGE("Texture not found (using white default): %s", textureName.c_str());
                LOGE("    ERROR!  This will cause the texture list to ray tracing to crash!");
                return { &m_TexWhite };
            }
            else
            {
                return { pTexture };
            }

        };

        // This Material ID is kind of a kludge to get the callback function to assign the correct buffer
        uint32_t MaterialID = Mat_Sphere;
        const auto& sceneMaterialLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef) -> std::optional<Material>
        {
            using namespace std::placeholders;
            return m_MaterialManager->CreateMaterial(*GetVulkan(), *pOpaqueShader, NUM_VULKAN_BUFFERS, 
                    std::bind(sceneTextureLoader, std::cref(materialDef), _1), 
                    std::bind(bufferLoader, MaterialID, _1));
        };

        // DrawableLoader::LoadDrawables(*GetVulkan(), *m_AssetManager, m_RenderPass, sRenderPassNames, gObjectName01, sceneMaterialLoader, m_SceneDrawables, {}, DrawableLoader::LoaderFlags::FindInstances/*|DrawableLoader::LoaderFlags::BakeTransforms*/, {}, gObjectScale01);
        // DrawableLoader::LoadDrawables(*GetVulkan(), *m_AssetManager, m_RenderPass, sRenderPassNames, gObjectName02, sceneMaterialLoader, m_SceneDrawables, {}, DrawableLoader::LoaderFlags::FindInstances/*|DrawableLoader::LoaderFlags::BakeTransforms*/, {}, gObjectScale02);

        // Can't use DrawableLoader::LoadDrawables because GLTF does not contain material information :(
        // So load the GLTF, set materials, and then create the drawable
        uint32_t loaderFlags = 0; // i.e. DrawableLoader::LoaderFlags::FindInstances
        bool ignoreTransforms = false;

        // Load the objects in scene
        float StepRadians = PI_MUL_2 / (float)gNumObjects;
        for (uint32_t WhichObject = 0; WhichObject < gNumObjects; WhichObject++)
        {
            float OneX = gSpacingRadius * cosf((float)WhichObject * StepRadians);
            float OneY = gObjectHeight;
            float OneZ = gSpacingRadius * sinf((float)WhichObject * StepRadians);

            // This is really the same object, just load it multiple times and will instance later
            std::vector<MeshObjectIntermediate> MeshObject = MeshObjectIntermediate::LoadGLTF(*m_AssetManager, gObjectName, false, gObjectScale);
            for (MeshObjectIntermediate& OneMesh : MeshObject)
            {
                OneMesh.m_Transform[3] = glm::vec4(OneX, OneY, OneZ, 1.0f);
                OneMesh.m_Materials[0].diffuseFilename = gObjectDiffuse;
                OneMesh.m_Materials[0].bumpFilename = std::string("stub_normal");
            }

            MaterialID = Mat_Sphere;
            DrawableLoader::CreateDrawables(*GetVulkan(), std::move(MeshObject), m_RenderPass, sRenderPassNames, sceneMaterialLoader, m_SceneDrawables, {}, loaderFlags, {});
            MeshObject.erase(MeshObject.begin(), MeshObject.end());
        }

        // Load the floor/base of the scene
        {
            // LOGI("Loading Ray Tracing mesh object: %s...", gFloorName);
            std::vector<MeshObjectIntermediate> MeshObject = MeshObjectIntermediate::LoadGLTF(*m_AssetManager, gFloorName, false, gFloorScale);
            for (MeshObjectIntermediate& OneMesh : MeshObject)
            {
                if (OneMesh.m_Materials.size() < 1)
                    OneMesh.m_Materials.resize(1);

                OneMesh.m_Transform = glm::rotate(OneMesh.m_Transform, -PI_DIV_2, glm::vec3(1.0f, 0.0f, 0.0f));
                OneMesh.m_Transform = glm::rotate(OneMesh.m_Transform, PI, glm::vec3(0.0f, 1.0f, 0.0f));

                OneMesh.m_Transform[3] = glm::vec4(gFloorPos, 1.0f);
                OneMesh.m_Materials[0].diffuseFilename = gFloorDiffuse;
                OneMesh.m_Materials[0].bumpFilename = std::string("stub_normal");
            }

            MaterialID = Mat_Floor;
            DrawableLoader::CreateDrawables(*GetVulkan(), std::move(MeshObject), m_RenderPass, sRenderPassNames, sceneMaterialLoader, m_SceneDrawables, {}, loaderFlags, {});
            MeshObject.erase(MeshObject.begin(), MeshObject.end());
        }

    }

    // **********************
    // Skybox
    // **********************
    {
        MeshObject mesh;
        if (LoadGLTF("./Media/Meshes/Skybox_Separate.gltf", 0, &mesh))
        {
            const auto* pSkyboxShader = m_ShaderManager->GetShader("Skybox");
            assert(pSkyboxShader);
            auto material = m_MaterialManager->CreateMaterial(*GetVulkan(), *pSkyboxShader, NUM_VULKAN_BUFFERS,
                [this](const std::string& texName) -> MaterialPass::tPerFrameTexInfo {
                    if (texName == "Environment")
                    {
                        return { m_TextureManager->GetTexture("Environment") };
                    }
                    else {
                        assert(0);
                        return {};
                    }
                },
                [this](const std::string& bufferName) -> tPerFrameVkBuffer {
                    if (bufferName == "Vert") {
                        return { m_SkyboxVertUniform[RP_GBUFFER][0].buf.GetVkBuffer() };
                    }
                    else {
                        assert(0);
                        return {};
                    }
                });
                uint32_t skyboxRenderPassBits = (1 << RP_LIGHT);// | (1 << RP_REFLECT);
                m_SkyboxDrawable = std::make_unique<Drawable>(*GetVulkan(), std::move(material));
                m_SkyboxDrawable->Init(m_RenderPass, sRenderPassNames, skyboxRenderPassBits, std::move(mesh));
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitSceneData()
//-----------------------------------------------------------------------------
{
    // Moved to CreateRayInputs()!

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitHdr()
//-----------------------------------------------------------------------------
{
    // Set the color profile
    VkHdrMetadataEXT AuthoringProfile = {};
    AuthoringProfile.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
    AuthoringProfile.displayPrimaryRed.x = 0.680f;
    AuthoringProfile.displayPrimaryRed.y = 0.320f;
    AuthoringProfile.displayPrimaryGreen.x = 0.265f;
    AuthoringProfile.displayPrimaryGreen.y = 0.690f;
    AuthoringProfile.displayPrimaryBlue.x = 0.150f;
    AuthoringProfile.displayPrimaryBlue.y = 0.060f;
    AuthoringProfile.whitePoint.x = 0.3127f;
    AuthoringProfile.whitePoint.y = 0.3290f;
    AuthoringProfile.maxLuminance = 80.0f;// 1000.f;
    AuthoringProfile.minLuminance = 0.001f;
    AuthoringProfile.maxContentLightLevel = 2000.f;
    AuthoringProfile.maxFrameAverageLightLevel = 1000.f;
    return GetVulkan()->SetSwapchainHrdMetadata(AuthoringProfile);
}

//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    // Gui
    assert(m_RenderPass[RP_HUD] != VK_NULL_HANDLE);
    m_Gui = std::make_unique<GuiImguiGfx<Vulkan>>(*GetVulkan(), m_RenderPass[RP_HUD]);
    if (!m_Gui->Initialize(windowHandle, m_HudRT[0].m_Width, m_HudRT[0].m_Height))
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
void Application::UpdateGui()
//-----------------------------------------------------------------------------
{
    if (gRenderHud && m_Gui)
    {
        // Update Gui
        m_Gui->Update();

        // Trying to decrease alpha on ImGui window but not working :9
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::GetStyle().Alpha = 1.0f;

        // Begin our window.
        static bool settingsOpen = true;
        ImGui::SetNextWindowSize(ImVec2((gRenderWidth * 3.0f) / 4.0f, 500.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(gRenderWidth / 8.0f, gRenderHeight / 2.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Settings", &settingsOpen, (ImGuiWindowFlags)0))
        {
            // if (ImGui::InputFloat3("Light Direction", &gLightDir.x, "%.1f"))
            // {
            //     uint32_t uiDebug = 5150;
            // }

            // if (ImGui::InputFloat4("Light Color", &gLightColorB.x, "%.1f"))
            // {
            //     uint32_t uiDebug = 5150;
            // }

            if (ImGui::InputInt("Max Bounces", (int *)&gSphereRayBounces))
            {
                uint32_t uiDebug = 5150;
            }

            if (ImGui::SliderFloat("Blend Amount", &gRayBlendAmount, 0.0f, 1.0f))
            {
                uint32_t uiDebug = 5150;
            }

            // if (ImGui::InputFloat("Blend Amount", &gRayBlendAmount))
            // {
            //     uint32_t uiDebug = 5150;
            // }

        }
        ImGui::End(); 

        if (ImGui::Begin("FPS", (bool*)nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            int offset = 0;
            ImGui::Text("FPS: %.1f", m_CurrentFPS);
        }
        ImGui::End();

        return;
    }
}

//-----------------------------------------------------------------------------
void Application::UpdateCamera(float ElapsedTime)
//-----------------------------------------------------------------------------
{
#ifdef OS_WINDOWS
    if (guiAPressed)
        m_SphereCamera.m_Yaw += CAMERA_ROTATE_SPEED * ElapsedTime;
    if (guiDPressed)
        m_SphereCamera.m_Yaw -= CAMERA_ROTATE_SPEED * ElapsedTime;

    if (!guiShiftPressed)
    {
        if (guiWPressed)
            m_SphereCamera.m_Distance -= CAMERA_ZOOM_SPEED * ElapsedTime;
        if (guiSPressed)
            m_SphereCamera.m_Distance += CAMERA_ZOOM_SPEED * ElapsedTime;
    }
    else
    {
        if (guiWPressed)
            m_SphereCamera.m_Pitch -= CAMERA_ROTATE_SPEED * ElapsedTime;
        if (guiSPressed)
            m_SphereCamera.m_Pitch += CAMERA_ROTATE_SPEED * ElapsedTime;
    }
#endif // OS_WINDOWS

    m_SphereCamera.Tick(ElapsedTime);

}


//-----------------------------------------------------------------------------
void Application::UpdateLighting(float ElapsedTime)
//-----------------------------------------------------------------------------
{
    if( m_sceneRayTracable )
    {
        if( m_sceneRayTracableCulled )
        {
            UpdateSceneRTCulled( *m_sceneRayTracableCulled );
        }
    }
}


//-----------------------------------------------------------------------------
void Application::UpdateSceneRTCulled( SceneRTCulled& sceneRTCulled )
//-----------------------------------------------------------------------------
{
    glm::vec3 lightPosition = glm::vec3(-208.f, 422.6f, 464.9f);    // Not really used
    BBoxTest lightBoundingBox( lightPosition, glm::vec3( 100.0f, 100.0f, 100.0f) );     // Not really used
    ViewFrustum viewFrustum(m_SphereCamera.ProjectionMatrix(), m_SphereCamera.ViewMatrix());
    FrustumTest viewFrustumTest( viewFrustum );

    sceneRTCulled.SetForceRegenerateAccelerationStructure( false );

    // Can NOT do culling because it breaks the GI lighting ray trace
    if( true )
    {
    // Update the ray tracing top level Acceleration Structure without culling (add everything to the AS).
        sceneRTCulled.Update( *m_sceneRayTracable, [&lightBoundingBox, &viewFrustumTest]( const glm::vec4& center, const glm::vec4& halfSize ) {
            return OctreeBase::eQueryResult::Inside;
        } );
    }
}

//-----------------------------------------------------------------------------
bool Application::ChangeSurfaceFormat(SurfaceFormat newSurfaceFormat)
//-----------------------------------------------------------------------------
{
    if (!GetVulkan()->ChangeSurfaceFormat(newSurfaceFormat))
    {
        return false;
    }

    // We need to modify the blit render pass (the only one that touches the output framebuffer).
    // RenderPass needs to be compatible with the framebuffer's format.
    // m_PassCmdBuffer[RP_BLIT] is good, gets rebuilt every frame
 
    for (auto& blitCmdBuffer : m_BlitCmdBuffer)
    {
        if (!blitCmdBuffer.Reset())
            return false;
    }

    vkDestroyRenderPass(GetVulkan()->m_VulkanDevice, m_RenderPass[RP_BLIT], nullptr);
    m_RenderPass[RP_BLIT] = VK_NULL_HANDLE;

    auto& PassSetup = m_PassSetup[RP_BLIT];
    PassSetup.ColorFormats = { GetVulkan()->m_SurfaceFormat };
    PassSetup.DepthFormat = { GetVulkan()->m_SwapchainDepth.format };

    if (!GetVulkan()->CreateRenderPass({ PassSetup.ColorFormats },
        PassSetup.DepthFormat,
        VK_SAMPLE_COUNT_1_BIT,
        PassSetup.ColorInputUsage,
        PassSetup.ColorOutputUsage,
        PassSetup.ClearDepthRenderPass,
        PassSetup.DepthOutputUsage,
        &m_RenderPass[RP_BLIT]))
        return false;

    if (!m_BlitDrawable->ReInit( m_RenderPass[RP_BLIT], sRenderPassNames[RP_BLIT], nullptr, nullptr))
    {
        return false;
    }

    // Rebuild the m_BlitCmdBuffer

    VkViewport Viewport = {};
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = (float)PassSetup.TargetWidth;
    Viewport.height = (float)PassSetup.TargetHeight;
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;

    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = PassSetup.TargetWidth;
    Scissor.extent.height = PassSetup.TargetHeight;

    for (uint32_t WhichBuffer = 0; WhichBuffer < GetVulkan()->m_SwapchainImageCount; ++WhichBuffer)
    {
        if (!m_BlitCmdBuffer[WhichBuffer].Begin(GetVulkan()->m_SwapchainBuffers[WhichBuffer].framebuffer, m_RenderPass[RP_BLIT], true/*swapchain renderpass*/))
        {
            return false;
        }

        vkCmdSetViewport(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Viewport);
        vkCmdSetScissor(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer, 0, 1, &Scissor);
    }
    // Add the blit commands
    AddDrawableToCmdBuffers(*m_BlitDrawable.get(), m_BlitCmdBuffer, 1, GetVulkan()->m_SwapchainImageCount);
    // End the blit command buffer
    for (uint32_t WhichBuffer = 0; WhichBuffer < GetVulkan()->m_SwapchainImageCount; WhichBuffer++)
    {
        if (!m_BlitCmdBuffer[WhichBuffer].End())
        {
            return false;
        }
    }

    if (!gRunOnHLM && !InitHdr())
        return false;

    return true;
}

//-----------------------------------------------------------------------------
Application::RenderPassData& Application::BeginRenderPass(RENDER_PASS WhichPass, uint32_t WhichBuffer, const std::span<const VkSemaphore> WaitSemaphores, const std::span<const VkPipelineStageFlags> WaitDstStageMasks, const std::span<const VkSemaphore> SignalSemaphores)
//-----------------------------------------------------------------------------
{
    auto* const pVulkan = GetVulkan();

    assert( WaitSemaphores.size() == WaitDstStageMasks.size() );

    RenderPassData& renderPassData = m_RenderPassSubmitData[WhichBuffer].emplace_back( RenderPassData{
        m_PassCmdBuffer[WhichBuffer][WhichPass].m_Name,
        WhichBuffer,
        WhichPass,
        m_PassCmdBuffer[WhichBuffer][WhichPass],
        {WaitSemaphores.begin(), WaitSemaphores.end()},
        {WaitDstStageMasks.begin(), WaitDstStageMasks.end()},
        {SignalSemaphores.begin(), SignalSemaphores.end()},
    } );

    const auto& passSetup = m_PassSetup[WhichPass];

    // Reset the primary command buffer...
    if (!renderPassData.CmdBuff.Reset())
    {
        // What should be done here?
    }

    // ... begin the primary command buffer ...
    if (!renderPassData.CmdBuff.Begin())
    {
        // What should be done here?
    }

    if( m_RenderPass[WhichPass] != VK_NULL_HANDLE )
    {
        VkFramebuffer Framebuffer;
        switch( WhichPass )
        {
        case RP_GBUFFER:
            Framebuffer = m_GBufferRT[0].m_FrameBuffer;
            break;
        case RP_LIGHT:
            Framebuffer = m_MainRT[0].m_FrameBuffer;
            break;
        case RP_HUD:
            Framebuffer = m_HudRT[0].m_FrameBuffer;
            break;
        case RP_BLIT:
            Framebuffer = pVulkan->m_SwapchainBuffers[WhichBuffer].framebuffer;
            break;
        default:
            assert( 0 );
        }

        VkRect2D PassArea = {};
        PassArea.offset.x = 0;
        PassArea.offset.y = 0;
        PassArea.extent.width = passSetup.TargetWidth;
        PassArea.extent.height = passSetup.TargetHeight;

        VkClearColorValue clearColor[1] = { passSetup.ClearColor[0], passSetup.ClearColor[1], passSetup.ClearColor[2], passSetup.ClearColor[3] };
        bool IsSwapChainRenderPass = WhichPass == RP_BLIT;
        VkSubpassContents SubContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

        renderPassData.CmdBuff.BeginRenderPass( PassArea, 0.0f, 1.0f, clearColor, (uint32_t) passSetup.ColorFormats.size(), passSetup.DepthFormat != TextureFormat::UNDEFINED, m_RenderPass[WhichPass], IsSwapChainRenderPass, Framebuffer, SubContents );
    }

    return renderPassData;
}

//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffers(const RenderPassData& renderPassData)
//-----------------------------------------------------------------------------
{
    std::vector<VkCommandBuffer> SubCommandBuffers;
    SubCommandBuffers.reserve(2);

    // Add sub commands to render list

    const uint32_t WhichBuffer = renderPassData.WhichBuffer;
    const uint32_t WhichPass = renderPassData.WhichPass;

    if (WhichPass == RP_LIGHT)
    {
        // Light cmd buffer is added before the m_ObjectCmdBuffer as there may be some objects rendering to the RP_LIGHT pass after the initial light pass (eg emissives)
        SubCommandBuffers.push_back(m_LightCmdBuffer[WhichBuffer].m_VkCommandBuffer);
        m_TotalDrawCalls += m_LightCmdBuffer[WhichBuffer].m_NumDrawCalls;
        m_TotalTriangles += m_LightCmdBuffer[WhichBuffer].m_NumTriangles;
    }

    if (m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_NumDrawCalls)
    {
        SubCommandBuffers.push_back(m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_VkCommandBuffer);
        m_TotalDrawCalls += m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_NumDrawCalls;
        m_TotalTriangles += m_ObjectCmdBuffer[WhichBuffer][WhichPass].m_NumTriangles;
    }

    if (WhichPass == RP_BLIT)
    {
        assert(SubCommandBuffers.empty()); // We currently don't handle m_ObjectCmdBuffer having commands to go in the blit pass, would need to recreate the cmd buffer when the renderpass changes (which will happen when the backbuffer surface format is switched)
        SubCommandBuffers.push_back(m_BlitCmdBuffer[WhichBuffer].m_VkCommandBuffer);
        m_TotalDrawCalls += m_BlitCmdBuffer[WhichBuffer].m_NumDrawCalls;
        m_TotalTriangles += m_BlitCmdBuffer[WhichBuffer].m_NumTriangles;
    }

    // Add all subcommands
    AddPassCommandBuffers(renderPassData, SubCommandBuffers);
}

//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffers(const RenderPassData& RenderPassData, std::span<VkCommandBuffer> SubCommandBuffers)
//-----------------------------------------------------------------------------
{
    // Make sure there is something to execue
    if (SubCommandBuffers.size() == 0)
    {
        // Technically, this may not be an error.  For now, let it fall through and submit nothing
        LOGE("Error! Being asked to add pass command buffers but nothing is in the list!");
    }
    else
    {
        vkCmdExecuteCommands(RenderPassData.CmdBuff.m_VkCommandBuffer, (uint32_t)SubCommandBuffers.size(), SubCommandBuffers.data());
    }
}

//-----------------------------------------------------------------------------
void Application::EndRenderPass(const RenderPassData& RenderPassData)
//-----------------------------------------------------------------------------
{
    // End the render pass.
    if( m_RenderPass[RenderPassData.WhichPass] != VK_NULL_HANDLE )
    {
        RenderPassData.CmdBuff.EndRenderPass();
    }
    // Stop recording the command buffer.
    RenderPassData.CmdBuff.End();
}

//-----------------------------------------------------------------------------
std::span<VkSemaphore> Application::SubmitGameThreadWork(uint32_t WhichBuffer, VkFence CompletionFence)
//-----------------------------------------------------------------------------
{
    VkResult RetVal = VK_SUCCESS;
    auto* const pVulkan = GetVulkan();

    const auto& SubmitList = m_RenderPassSubmitData[WhichBuffer];
    uint32_t NumCmdBuffers = (uint32_t)SubmitList.size();

#ifdef ENABLE_PROFILING
    PROFILE_ENTER(GROUP_VKFRAMEWORK, 0, "SubmitGameThreadWork: %d Buffers", NumCmdBuffers);
#endif // ENABLE_PROFILING

    const uint32_t NumPasses = (uint32_t) SubmitList.size();

    for(uint32_t WhichPass = 0; WhichPass < NumPasses; ++WhichPass)
    {
        auto& OneSubmit = SubmitList[WhichPass];
        bool LastPass = (WhichPass == NumPasses - 1);        

#ifdef ENABLE_PROFILING
        PROFILE_ENTER(GROUP_VKFRAMEWORK, 0, "Submit Cmd Buffer: %s", OneSubmit.Desc.c_str());
#endif // ENABLE_PROFILING

        OneSubmit.CmdBuff.QueueSubmitT( OneSubmit.WaitSemaphores, OneSubmit.WaitDstStageMasks, OneSubmit.SignalSemaphores, LastPass ? CompletionFence : VK_NULL_HANDLE );

#ifdef ENABLE_PROFILING
        PROFILE_EXIT(GROUP_VKFRAMEWORK);    // "Submit Cmd Buffer: %s"
#endif // ENABLE_PROFILING
    }

#ifdef ENABLE_PROFILING
    PROFILE_EXIT(GROUP_VKFRAMEWORK);    // "SubmitGameThreadWork: %d Buffers"
#endif // ENABLE_PROFILING

    return !m_RenderPassSubmitData[WhichBuffer].empty() ? std::span<VkSemaphore>{m_RenderPassSubmitData[WhichBuffer].rbegin()->SignalSemaphores} : std::span<VkSemaphore>{};
}
