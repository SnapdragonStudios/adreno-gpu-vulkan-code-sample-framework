//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "meshObjectRT.hpp"
#include "memory/memoryManager.hpp"
#include "material/vertexFormat.hpp"
#include "material/vulkan/vertexDescription.hpp"
#include "mesh/meshHelper.hpp"
#include "mesh/meshIntermediate.hpp"
#include "vulkan/MeshObject.h"
#include "vulkanRT.hpp"
#include <glm/gtc/type_ptr.hpp>

const VertexFormat MeshObjectRT::accelerationStructureVertexFormat = { 12, VertexFormat::eInputRate::Vertex, { VertexFormat::Element{ 0, VertexFormat::Element::ElementType::t::Vec3} }, {"Position"} };
const VkFormat MeshObjectRT::accelerationStructureVkVertexFormat = VertexDescription::VkFormatFromElementType(MeshObjectRT::accelerationStructureVertexFormat.elements.front().type);


MeshObjectRT::MeshObjectRT() : AccelerationStructureUpdateable(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
{}


MeshObjectRT::MeshObjectRT(MeshObjectRT&& other)
    : AccelerationStructureUpdateable(std::move(other))
    , m_DeviceAddress(other.m_DeviceAddress)
    , m_AABBMin(other.m_AABBMin)
    , m_AABBMax(other.m_AABBMax)
{
    other.m_DeviceAddress = 0;
    other.m_AABBMin = {};
    other.m_AABBMax = {};
}


MeshObjectRT::~MeshObjectRT()
{}


bool MeshObjectRT::Create( Vulkan& vulkan, VulkanRT& vulkanRT, const MeshObjectIntermediate& meshObject, bool allowDataAccess, MeshObjectRT::UpdateMode updateMode)
{
    auto& memoryManager = vulkan.GetMemoryManager();

    // Convert the 'intermediate' mesh vertex data in to 'vertexFormat' and then copy in to a VertexBuffer (device memory)
    auto deviceVertexBuffer = CreateRtVertexBuffer(memoryManager, meshObject);
    if (deviceVertexBuffer.GetVkBuffer() == VK_NULL_HANDLE)
    {
        return false;
    }

    // Get the AABB size.
    m_AABBMin = {};
    m_AABBMax = {};
    if( !meshObject.m_VertexBuffer.empty() )
    {
        m_AABBMin = glm::make_vec3( meshObject.m_VertexBuffer[0].position );
        m_AABBMax = m_AABBMin;
        for( const auto& vert : meshObject.m_VertexBuffer )
        {
            const auto position = glm::make_vec3( vert.position );
            m_AABBMin = glm::min( m_AABBMin, position );
            m_AABBMax = glm::max( m_AABBMax, position );
        }
    }

    const uint32_t numVertices = (uint32_t) meshObject.m_VertexBuffer.size();

    //
    // If meshObject has an index buffer then copy the data in to a Vulkan buffer
    //
    std::optional<IndexBuffer<Vulkan>> deviceIndexBuffer;
    if (!MeshHelper::CreateIndexBuffer(memoryManager, meshObject, deviceIndexBuffer, BufferUsageFlags::AccelerationStructureBuild|BufferUsageFlags::ShaderDeviceAddress))
    {
        return false;
    }
    VkIndexType indexType;
    uint32_t primitiveCount;

    if (deviceIndexBuffer)
    {
        indexType = deviceIndexBuffer->GetVkIndexType();
        primitiveCount = (uint32_t)deviceIndexBuffer.value().GetNumIndices() / 3;
    }
    else
    {
        indexType = VK_INDEX_TYPE_NONE_KHR;
        primitiveCount = (uint32_t)meshObject.m_VertexBuffer.size() / 3;
    }

    // Build initial information for the geometry (for getting buffer/build sizes).  Will be filled in further (later) for the build of the acceleration structure.
    std::array<VkAccelerationStructureGeometryKHR, 1> asGeometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    asGeometry[0].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeometry[0].flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    asGeometry[0].geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    asGeometry[0].geometry.triangles.vertexFormat = accelerationStructureVkVertexFormat;
    asGeometry[0].geometry.triangles.vertexStride = (VkDeviceSize)accelerationStructureVertexFormat.span;
    asGeometry[0].geometry.triangles.maxVertex = numVertices;
    asGeometry[0].geometry.triangles.indexType = indexType;

    VkBuildAccelerationStructureFlagsKHR asFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR /*| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR*/;
    if (updateMode != UpdateMode::NotUpdatable)
        asFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

    if (allowDataAccess)
        asFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR ;

    std::array<uint32_t, asGeometry.size()> asMaxPrimitiveCounts = { primitiveCount };

    // Create the Acceleration structue and its backing buffer.  Potentially creates a second AS too if the VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR flag is set and updateMode PingPong (two buffers needed for updates).
    if (!AccelerationStructureUpdateable::Create(vulkan, vulkanRT, asFlags, updateMode, asGeometry, asMaxPrimitiveCounts))
    {
        return false;
    }

    // Make the acceleration structure build scratch buffer.
    AccelerationStructureScratch asBuildScratch;
    if (!asBuildScratch.Create(vulkan, GetBuildScratchSize()))
    {
        AccelerationStructureUpdateable::Destroy(vulkan, vulkanRT);
        return false;
    }

    // Fill in the missing information in geometry structures now we are ready to build it!
    asGeometry[0].geometry.triangles.vertexData.deviceAddress = memoryManager.GetBufferDeviceAddress(deviceVertexBuffer);
    if (deviceIndexBuffer)
    {
        asGeometry[0].geometry.triangles.indexData.deviceAddress = memoryManager.GetBufferDeviceAddress(deviceIndexBuffer.value());
    }

    // Range info for the geometry we want to build
    std::array<VkAccelerationStructureBuildRangeInfoKHR, 1> asBuildRangeInfo = {};
    asBuildRangeInfo[0].primitiveCount = primitiveCount;

    // Use the GPU to (immediately) build the acceleration structure.
    {
        VkCommandBuffer commandBuffer = vulkan.StartSetupCommandBuffer();
        BuildAccelerationStructuresCmd(vulkanRT, commandBuffer, asGeometry, asBuildRangeInfo, asBuildScratch);
        vulkan.FinishSetupCommandBuffer(commandBuffer);
    }

    // Acceleration structure is ready.  Grab its device address (for the top level structure to reference)
    assert(!m_DeviceAddress);
    m_DeviceAddress = vulkanRT.vkGetAccelerationStructureDeviceAddressKHR( GetAccelerationStructure().GetVkAccelerationStructure() );

    // Cleanup
    asBuildScratch.Destroy(vulkan);

    return true;
}


void MeshObjectRT::Destroy(Vulkan& vulkan, VulkanRT& vulkanRT)
{
    AccelerationStructureUpdateable::Destroy(vulkan, vulkanRT);
    m_DeviceAddress = 0;
}


Buffer<Vulkan> MeshObjectRT::CreateRtVertexBuffer(MemoryManager<Vulkan>& memoryManager, const MeshObjectIntermediate& meshObject, BufferUsageFlags usageFlags)
{
    Buffer<Vulkan> deviceVertexBuffer;
    {
        const std::vector<uint32_t> vertexData = MeshObjectIntermediate::CopyFatVertexToFormattedBuffer(meshObject.m_VertexBuffer, {}, accelerationStructureVertexFormat);
        if (!deviceVertexBuffer.Initialize(&memoryManager, vertexData.size() * sizeof(uint32_t), usageFlags, vertexData.data()))
        {
            return {};
        }
    }
    return deviceVertexBuffer;
}
