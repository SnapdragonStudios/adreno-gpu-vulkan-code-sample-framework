//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "meshUpdateRT.hpp"
#include "meshObjectRT.hpp"
#include "vulkan/MeshObject.h"
#include "mesh/meshHelper.hpp"
#include "mesh/meshIntermediate.hpp"
#include "material/vertexFormat.hpp"


MeshUpdateRT::MeshUpdateRT(MeshUpdateRT&& other) noexcept
    : m_VertexBuffer(std::move(other.m_VertexBuffer))
    , m_IndexBuffer(std::move(other.m_IndexBuffer))
    , m_UpdateScratch(std::move(other.m_UpdateScratch))
    , m_VertexBufferDeviceAddress(other.m_VertexBufferDeviceAddress)
    , m_IndexBufferDeviceAddress(other.m_IndexBufferDeviceAddress)
    , m_PrimitiveCount(other.m_PrimitiveCount)
    , m_NumVertices(other.m_NumVertices)
{
    other.m_VertexBufferDeviceAddress = 0;
    other.m_IndexBufferDeviceAddress = 0;
    other.m_PrimitiveCount = 0;
    other.m_NumVertices = 0;
}

MeshUpdateRT::~MeshUpdateRT()
{
}


bool MeshUpdateRT::Create(Vulkan& vulkan, const MeshObjectIntermediate& meshObject, size_t updateScratchSize)
{
    auto& memoryManager = vulkan.GetMemoryManager();

    m_VertexBuffer.Destroy();
    if (m_IndexBuffer.has_value())
        m_IndexBuffer->Destroy();
    m_IndexBuffer.reset();
    m_PrimitiveCount = 0;
    m_NumVertices = 0;
    m_VertexBufferDeviceAddress = {};
    m_IndexBufferDeviceAddress = {};

    // Convert the 'intermediate' mesh vertex data in to 'vertexFormat' and then copy in to a VertexBuffer (device memory).  Make WRITABLE from compute.
    // We COULD recycle the Buffer created by MeshObjectRT (with the storage flag added) but although this is slower it is also cleaner!
    m_VertexBuffer = MeshObjectRT::CreateRtVertexBuffer(memoryManager, meshObject, BufferUsageFlags::AccelerationStructureBuild| BufferUsageFlags::Storage|BufferUsageFlags::ShaderDeviceAddress);
    if (m_VertexBuffer.GetVkBuffer() == VK_NULL_HANDLE)
        return false;
    m_VertexBufferDeviceAddress = memoryManager.GetBufferDeviceAddress(m_VertexBuffer);
    m_NumVertices = (uint32_t)meshObject.m_VertexBuffer.size();

    // Get the index buffer from the intermediate mesh (if it has one).  Again we could recycle the index buffer created by the MeshObjectRt but cleaner to create independantly.
    if (!MeshHelper::CreateIndexBuffer<Vulkan>(memoryManager, meshObject, m_IndexBuffer, BufferUsageFlags::AccelerationStructureBuild | BufferUsageFlags::ShaderDeviceAddress))
    {
        return false;
    }
    if (m_IndexBuffer)
    {
        m_IndexBufferDeviceAddress = memoryManager.GetBufferDeviceAddress(*m_IndexBuffer);
        m_PrimitiveCount = (uint32_t)m_IndexBuffer.value().GetNumIndices() / 3;
    }
    else
    {
        m_PrimitiveCount = m_NumVertices / 3;
    }

    // Create (and store) a scratch buffer for updates (if requested)
    if (updateScratchSize>0 && !m_UpdateScratch.Create(vulkan, updateScratchSize))
    {
        return false;
    }

    return true;
}


void MeshUpdateRT::Destroy(Vulkan& vulkan, VulkanRT& vulkanRT)
{
    m_UpdateScratch.Destroy(vulkan);
}

// 
// Setup geometry data for updating an existing AS using new vertex data.
//
void MeshUpdateRT::PrepareUpdateASGeometryData(VkAccelerationStructureGeometryKHR& asGeometry, VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo) const
{
    const VkIndexType indexType = (m_IndexBuffer) ? m_IndexBuffer->GetVkIndexType() : VK_INDEX_TYPE_NONE_KHR;

    // Build initial information for the geometry (for getting buffer/build sizes).  Will be filled in further (later) for the build of the acceleration structure.
    asGeometry = VkAccelerationStructureGeometryKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    asGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    asGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    asGeometry.geometry.triangles.vertexFormat = MeshObjectRT::accelerationStructureVkVertexFormat;
    asGeometry.geometry.triangles.vertexData.deviceAddress = m_VertexBufferDeviceAddress;
    asGeometry.geometry.triangles.vertexStride = (VkDeviceSize) MeshObjectRT::accelerationStructureVertexFormat.span;
    asGeometry.geometry.triangles.maxVertex = m_NumVertices;
    asGeometry.geometry.triangles.indexType = indexType;
    asGeometry.geometry.triangles.indexData.deviceAddress = m_IndexBufferDeviceAddress;

    asBuildRangeInfo = { .primitiveCount = m_PrimitiveCount };
}



void MeshUpdateRT::PrepareUpdateASData(VkAccelerationStructureBuildGeometryInfoKHR& asBuildGeometryInfo, VkAccelerationStructureGeometryKHR& asGeometry, VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo, const AccelerationStructureUpdateable& accelerationStructure) const
{
    PrepareUpdateASGeometryData(asGeometry, asBuildRangeInfo);
    accelerationStructure.PrepareUpdateAccelerationStructuresCmd( asBuildGeometryInfo, { &asGeometry, 1 }, m_UpdateScratch);
}

void MeshUpdateRT::UpdateAS(VulkanRT& vulkanRT, VkCommandBuffer cmdBuffer, const AccelerationStructureUpdateable& accelerationStructure) const
{
    std::array<VkAccelerationStructureGeometryKHR, 1> asGeometry;
    std::array<VkAccelerationStructureBuildRangeInfoKHR, 1> asBuildRangeInfo;
    PrepareUpdateASGeometryData(asGeometry[0], asBuildRangeInfo[0]);
    accelerationStructure.UpdateAccelerationStructuresCmd(vulkanRT, cmdBuffer, asGeometry, asBuildRangeInfo, m_UpdateScratch);
}
