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


// Class
class MeshObjectRT final : public AccelerationStructureUpdateable
{
    MeshObjectRT& operator=(const MeshObjectRT&) = delete;
    MeshObjectRT(const MeshObjectRT&) = delete;
public:
    MeshObjectRT();
    MeshObjectRT(MeshObjectRT&& other);
    ~MeshObjectRT();

    bool Create(Vulkan& vulkan, VulkanRT& vulkanRT, const MeshObjectIntermediate& meshObject, UpdateMode updateMode = UpdateMode::NotUpdatable);
    void Destroy(Vulkan& vulkan, VulkanRT& vulkanRT) override;

    uint64_t GetVkDeviceAddress() const { return m_DeviceAddress; }
    std::pair<glm::vec3, glm::vec3> GetAABB() const { return { m_AABBMin, m_AABBMax }; };

    static BufferT<Vulkan> CreateRtVertexBuffer(MemoryManager<Vulkan>& memoryManager, const MeshObjectIntermediate& meshObject, BufferUsageFlags usageFlags = BufferUsageFlags::AccelerationStructureBuild|BufferUsageFlags::ShaderDeviceAddress);

    static const VertexFormat accelerationStructureVertexFormat;
    static const VkFormat accelerationStructureVkVertexFormat;

private:
    uint64_t m_DeviceAddress = 0;

    glm::vec3 m_AABBMin = {};
    glm::vec3 m_AABBMax = {};
};
