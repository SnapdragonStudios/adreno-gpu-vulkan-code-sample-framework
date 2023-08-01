//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "accelerationStructure.hpp"
#include "memory/vulkan/bufferObject.hpp"
#include "memory/vulkan/indexBufferObject.hpp"
#include <system/glm_common.hpp>

// Forward declarations
class MeshObjectIntermediate;
class VertexFormat;
class VulkanRT;

/// @brief Base class providing the ability to update bottom level acceleration structure (BLAS) vertex data.
/// Contains a buffer with the mesh vertex and index data ready to be modified and then used in a build/update of an existing BLAS (if the BLAS does not have the update flag enabled then it will be built rather than updated)
/// Create a derived (app specific) class if the 'original' vertex data is required.
class MeshUpdateRT
{
    MeshUpdateRT(const MeshUpdateRT&) = delete;
    MeshUpdateRT& operator=(const MeshUpdateRT&) = delete;

public:
    MeshUpdateRT() {};
    MeshUpdateRT(MeshUpdateRT&& other) noexcept;
    ~MeshUpdateRT();

    virtual bool Create(Vulkan& vulkan, const MeshObjectIntermediate& meshObject, size_t updateScratchSize);
    virtual void Destroy(Vulkan& vulkan, VulkanRT& vulkanRT);

    // Populate the data structures required to do an update of the given bottom level acceleration structure (BLAS).  Same as @UpdateAS but does not record the vulkan command (can be used to batch together multiple updates into one vulkan command).
    void PrepareUpdateASData(VkAccelerationStructureBuildGeometryInfoKHR&, VkAccelerationStructureGeometryKHR&, VkAccelerationStructureBuildRangeInfoKHR&, const AccelerationStructureUpdateable& bottomLevelAccelerationStructure) const;

    /// Record the vulkan command to update the given bottom level acceleration structure (BLAS).
    void UpdateAS(VulkanRT& vulkanRT, VkCommandBuffer cmdBuffer, const AccelerationStructureUpdateable& bottomLevelAccelerationStructure) const;

    VkBuffer GetVertexVkBuffer() const { return m_VertexBuffer.GetVkBuffer(); }
    uint32_t GetNumVertices() const { return m_NumVertices; }
    uint32_t GetPrimitiveCount() const { return m_PrimitiveCount; }
private:
    // internal use
    void PrepareUpdateASGeometryData(VkAccelerationStructureGeometryKHR& asGeometry, VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo) const;

protected:
    // Variables needed for Updates
    BufferT<Vulkan> m_VertexBuffer;                        
    std::optional<IndexBufferObject> m_IndexBuffer;     // optional, depending on meshObject having indices
    AccelerationStructureScratch m_UpdateScratch;
    VkDeviceAddress m_VertexBufferDeviceAddress = {};
    VkDeviceAddress m_IndexBufferDeviceAddress = {};
    uint32_t m_PrimitiveCount = 0;
    uint32_t m_NumVertices = 0;
};
