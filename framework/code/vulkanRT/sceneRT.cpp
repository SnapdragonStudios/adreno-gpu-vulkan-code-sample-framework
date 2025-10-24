//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "sceneRT.hpp"
#include "vulkanRT.hpp"
#include "accelerationInstanceBufferObject.hpp"
#include <iterator>
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include "system/os_common.h"

SceneRTBase::SceneRTBase()
{}

SceneRT::SceneRT(Vulkan& vulkan, VulkanRT& vulkanRT)
    : SceneRTBase(), AccelerationStructureUpdateable(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
    , m_vulkan(vulkan)
    , m_vulkanRT(vulkanRT)
{}

// NOT CURRENTLY IMPLEMENTED!  Is defined (and skeleton implemented) so vector<SceneRT> compiles, but any attempt to resize the vector will fail (always reserve to the max number required before use).
SceneRT::SceneRT( SceneRT&& other ) noexcept
    : AccelerationStructureUpdateable(std::move(other))
    , m_vulkan(other.m_vulkan)
    , m_vulkanRT(other.m_vulkanRT)
{
    assert( 0 );
}

SceneRT::~SceneRT()
{
    auto& memoryManager = m_vulkan.GetMemoryManager();
    for( auto& blo : m_bottomLevelObjects )
    {
        blo.second.Destroy( m_vulkan, m_vulkanRT );
    }
    for (auto& blo : m_newBottomLevelObjects)
    {
        blo.second.Destroy(m_vulkan, m_vulkanRT);
    }
    m_accelerationBuildScratch.Destroy(m_vulkan);
    AccelerationStructureUpdateable::Destroy(m_vulkan, m_vulkanRT);
}

uint64_t SceneRT::AddObject(MeshObjectRT object)
{
    uint64_t id = object.GetVkDeviceAddress();
    m_newBottomLevelObjects.insert( { id, std::move( object ) } );
    return id;
}

const MeshObjectRT* SceneRT::FindObject(uint64_t id) const
{
    const auto it = m_newBottomLevelObjects.find(id);
    if (it != m_newBottomLevelObjects.end())
        return &it->second;
    return nullptr;
}

void SceneRT::AddInstance(uint64_t id, glm::mat3x4 transform)
{
    m_newInstances.emplace_back(tIdAndTransformPair{ id, transform } );
    m_pendingInstancesUpdate = true;
}

void SceneRT::RemoveAllInstances()
{
    m_newInstances.clear();
    m_pendingInstancesUpdate = !m_instances.empty();
}

bool SceneRT::CreateAccelerationStructure( UpdateMode updateMode, size_t minSize)
{
    auto& memoryManager = m_vulkan.GetMemoryManager();

    if( !m_newInstances.empty() )
    {
        std::swap( m_newInstances, m_instances );
        m_newInstances.clear();
    }

    // Create array of VkGeometryInstanceKHR (one entry per instance)
    if (!m_instanceBuffer.Initialize(&memoryManager, std::max( m_instances.size(), minSize ) ))
    {
        return false;
    }
    m_instanceBufferDeviceAddress = memoryManager.GetBufferDeviceAddress( m_instanceBuffer );

    m_accelerationBuildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    if (updateMode != UpdateMode::NotUpdatable)
        m_accelerationBuildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;    

    {
        // Populate the instance buffer data
        if( !m_instances.empty() )
        {
            auto mapped = m_instanceBuffer.Map();
            VkAccelerationStructureInstanceKHR* pData = mapped.data();

            // For ray tracing scene data to get the correct object we need to set the
            // custom index.  Using sequential count as array index in shader.
            // This is the value read in shader with "rayQueryGetIntersectionInstanceCustomIndexEXT" (gl_InstanceCustomIndexEXT)
            uint32_t CustomIndex = 0;
            for( const auto& [bottomLevelObjectAddr, mat3x4] : m_instances )
            {
                pData->instanceCustomIndex = CustomIndex++;
                pData->mask = 1;
                pData->instanceShaderBindingTableRecordOffset = 0;
                pData->flags = 0;// VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;// | VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;
                pData->accelerationStructureReference = bottomLevelObjectAddr;
                memcpy( &pData->transform.matrix, &mat3x4, sizeof( pData->transform.matrix ) );
                ++pData;
            }
        }
        m_instanceBufferNumUsed = m_instances.size();
    }

    // Build initial information (for getting buffer/build sizes).  Will be filled in further (later) for the build of the acceleration structure.
    std::array<VkAccelerationStructureGeometryKHR, 1> asGeometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    asGeometry[0].geometryType = (VkGeometryTypeKHR) 2;//change back to VK_GEOMETRY_TYPE_INSTANCES_KHR when Android vulkan headers get updated to not have VK_GEOMETRY_TYPE_INSTANCES_KHR = 1000150000;
    asGeometry[0].flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    asGeometry[0].geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

    std::array<uint32_t, asGeometry.size()> asMaxInstanceCounts = { (uint32_t)m_instanceBuffer.GetNumInstances() };

    // Create the acceleration structure(s) ready for building
    if (!AccelerationStructureUpdateable::Create(m_vulkan, m_vulkanRT, m_accelerationBuildFlags, updateMode, asGeometry, asMaxInstanceCounts))
    {
        return false;
    }

    // Make the acceleration structure build scratch buffer.
    if (!m_accelerationBuildScratch.Create(m_vulkan, GetBuildScratchSize()))
    {
        AccelerationStructureUpdateable::Destroy(m_vulkan, m_vulkanRT);
    }

    // Fill in the missing information in geometry structures now we are ready to build it!
    asGeometry[0].geometry.instances.data.deviceAddress = memoryManager.GetBufferDeviceAddress(m_instanceBuffer);

    // Range info for the instances we want to build
    std::array<VkAccelerationStructureBuildRangeInfoKHR, 1> asBuildRangeInfo = {};
    asBuildRangeInfo[0].primitiveCount = (uint32_t)m_instanceBufferNumUsed;

    // Use the GPU to (immediately) build the acceleration structure.
    {
        VkCommandBuffer commandBuffer = m_vulkan.StartSetupCommandBuffer();
        BuildAccelerationStructuresCmd(m_vulkanRT, commandBuffer, asGeometry, asBuildRangeInfo, m_accelerationBuildScratch);

        m_vulkan.FinishSetupCommandBuffer(commandBuffer);
    }

    return true;
}

void SceneRT::Update( bool forceRegeneration )
{
    // Add any new objects.
    const bool regenerateBottomLevelObjects = !m_newBottomLevelObjects.empty();
    if( regenerateBottomLevelObjects )
    {
        if (m_bottomLevelObjects.empty())
        {
            std::swap(m_newBottomLevelObjects, m_bottomLevelObjects);
        }
        else
        {
            std::move( m_newBottomLevelObjects.begin(), m_newBottomLevelObjects.end(), std::inserter( m_bottomLevelObjects, m_bottomLevelObjects.end() ) );
        }
        m_newBottomLevelObjects.clear();
        m_accelerationStructurePendingGpuRebuild = true;
    }

    // Add new instances (replaces the existing instances)
    bool regenerateInstances = m_pendingInstancesUpdate;
    if( regenerateInstances )
    {
        std::swap( m_newInstances, m_instances );
        m_newInstances.clear();
        m_pendingInstancesUpdate = false;
    }

    if (regenerateInstances || forceRegeneration)
    {
        //
        // Re-write the instance buffer (contents have changed).
        //

        if( m_instances.size() > m_instanceBuffer.GetNumInstances() )
        {
            // Buffer needs resizing.
            LOGE( "Raytrace m_InstanceBuffer not big enough to hold new instances.  Implement buffer resizing or increase minSize passed in to CreateAccelerationStructure." );
            return;
        }

        if( !m_instances.empty() )
        {
            auto mapped = m_instanceBuffer.Map();
            VkAccelerationStructureInstanceKHR* pData = mapped.data();

            // For ray tracing scene data to get the correct object we need to set the
            // custom index.  Using sequential count as array index in shader.
            // This is the value read in shader with "rayQueryGetIntersectionInstanceCustomIndexEXT" (gl_InstanceCustomIndexEXT)
            uint32_t CustomIndex = 0;
            for( const auto& [bottomLevelObjectAddr, mat3x4] : m_instances )
            {
                pData->instanceCustomIndex = CustomIndex++;
                pData->mask = 1;
                pData->instanceShaderBindingTableRecordOffset = 0;
                pData->flags = 0;// VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;// | VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;
                pData->accelerationStructureReference = bottomLevelObjectAddr;
                //glm::mat3x4 mat3x4{ 1.0f/*identity matrix*/ };
                memcpy( &pData->transform.matrix, &mat3x4, sizeof( pData->transform.matrix ) );
                ++pData;
            }
        }
        m_instanceBufferNumUsed = m_instances.size();
        m_accelerationStructurePendingGpuRebuild = true;
    }
}

void SceneRT::UpdateAS( VkCommandBuffer cmdBuffer, bool forceASUpdate )
{
    //
    // Create any GPU commands to update/rebuild buffers
    //

    if( !m_accelerationStructurePendingGpuRebuild && !forceASUpdate)
        return;
    bool rebuildAccelerationStructure = m_accelerationStructurePendingGpuRebuild;

    // If the buffer is not built for update do a build instead.
    if ((m_accelerationBuildFlags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR) == 0)
        rebuildAccelerationStructure = true;

    // 
    // Build the Vulkan structures for the vkCmdBuildAccelerationStructureKHR.
    // This time we are updating or rebuilding the AS that was already built once by CreateAccelerationStructure
    //

    std::array<VkAccelerationStructureGeometryKHR, 1> asGeometry = {};
    asGeometry[0].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    asGeometry[0].geometryType = (VkGeometryTypeKHR) 2;//change back to VK_GEOMETRY_TYPE_INSTANCES_KHR when Android vulkan headers get updated to not have VK_GEOMETRY_TYPE_INSTANCES_KHR = 1000150000;;
    asGeometry[0].geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    asGeometry[0].geometry.instances.arrayOfPointers = VK_FALSE;
    asGeometry[0].geometry.instances.data.deviceAddress = m_instanceBufferDeviceAddress;

    // Range info for the instances we want to build
    std::array<VkAccelerationStructureBuildRangeInfoKHR, 1> asBuildRangeInfo = {};
    asBuildRangeInfo[0].primitiveCount = (uint32_t)m_instanceBufferNumUsed;

    if (rebuildAccelerationStructure)
    {
        BuildAccelerationStructuresCmd(m_vulkanRT, cmdBuffer, asGeometry, asBuildRangeInfo, m_accelerationBuildScratch);
    }
    else
    {
        UpdateAccelerationStructuresCmd(m_vulkanRT, cmdBuffer, asGeometry, asBuildRangeInfo, m_accelerationBuildScratch);
    }

    m_accelerationStructurePendingGpuRebuild = false;
}


SceneRTCullable::SceneRTCullable( Vulkan& vulkan, VulkanRT& vulkanRT) 
    : SceneRT( vulkan, vulkanRT)
    , m_Octree( glm::vec3(0.0f), glm::vec3(8192.0f, 8192.0f, 8192.0f), 4096)
{
}

void SceneRTCullable::AddInstance( uint64_t id, glm::mat3x4 transform )
{
    auto bottomLevelObjectIt = m_newBottomLevelObjects.find( id );
    if( bottomLevelObjectIt == m_newBottomLevelObjects.end() )
    {
        bottomLevelObjectIt = m_bottomLevelObjects.find( id );
        if( bottomLevelObjectIt == m_bottomLevelObjects.end() )
        {
            LOGE( "Bottom level RT instance object %" PRIu64 " was not registered with AddObject", id );
            return;
        }
    }

    m_knownInstances.push_back( { id, transform } );

    const auto [aabbMin, aabbMax] = bottomLevelObjectIt->second.GetAABB();
    const std::array<glm::vec3, 2> aabbMinMax = { aabbMin, aabbMax };

    glm::vec3 instanceMin = glm::vec4( aabbMinMax[0], 1.0f ) * transform;
    glm::vec3 instanceMax = instanceMin;
    for( int i = 1; i < 8; i++ )
    {
        glm::vec3 newVal = glm::vec4( aabbMinMax[i>>2].x, aabbMinMax[(i>>1)&1].y, aabbMinMax[i&1].z, 1.0f ) * transform;
        instanceMin = glm::min( instanceMin, newVal );
        instanceMax = glm::max( instanceMax, newVal );
    }

    const auto instanceAabbCenter = (instanceMin + instanceMax) * 0.5f;
    const auto instanceAabbSize = instanceMax - instanceMin;

    m_Octree.AddObject( glm::vec4( instanceAabbCenter, 1.0f ), glm::vec4( instanceAabbSize, 0.0f ), { (uint32_t)(m_knownInstances.size() - 1) } );
    //m_Octree.AddObject( glm::vec4( transform[0].w, transform[1].w, transform[2].w, 1.0f ), glm::vec4( aabbWidth, 0.0f ), { (uint32_t)(m_knownInstances.size() - 1) } );
}


bool SceneRTCullable::CreateAccelerationStructure()
{
    for (const auto& instance : m_knownInstances)
        SceneRT::AddInstance( instance.modelId, instance.transform );
    Update( true );

    return SceneRT::CreateAccelerationStructure( UpdateMode::NotUpdatable/*SceneRTCulled should be used for an 'updatable' TLAS*/, m_knownInstances.size());
}


SceneRTCulled::SceneRTCulled( Vulkan& vulkan, VulkanRT& vulkanRT) 
    : SceneRT(vulkan, vulkanRT)
    , m_numVisibleInstanceIndices(0)
    , m_forceRegenerateAccelerationStructure(false)
    , m_forceUpdateAccelerationStructure(false)
{
}

void SceneRTCulled::PostQueryUpdate(const SceneRTCullable& scene, bool regenerateInstances)
{
    if( regenerateInstances )
    {
        if (m_numVisibleInstanceIndices > 0)
        {
            // Add all the visible instances (will cause the SceneRT::Update to regenerate the acceleration structure data).
            if (m_numVisibleInstanceIndices == scene.m_knownInstances.size())
            {
                // If no instances are culled, add them in the order they are already listed.
                // This way the GI stuff that expects them in a certain order will still work.
                // The bottom line is if anything is culled they will get added in the order they
                // show up in the sorting tree (breaking GI).
                for (const auto& inst : scene.m_knownInstances)
                {
                    SceneRT::AddInstance(inst.modelId, inst.transform);
                }
            }
            else
            {
                // Add the visible instances in the order defined by m_visibleInstanceIndices (culler output) 
                for (size_t i = 0; i < m_numVisibleInstanceIndices; ++i)
                {
                    const auto& inst = scene.m_knownInstances[m_visibleInstanceIndices[i]];
                    SceneRT::AddInstance(inst.modelId, inst.transform);
                }
            }
        }
        else
        {
            // Special case of no visible instances.
            SceneRT::RemoveAllInstances();
        }
    }

    SceneRT::Update( m_forceRegenerateAccelerationStructure );
}

