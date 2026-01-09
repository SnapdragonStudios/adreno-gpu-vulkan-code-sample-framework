//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "accelerationStructure.hpp"
#include "vulkanRT.hpp"


bool AccelerationStructure<Vulkan>::Create(Vulkan& vulkan, VulkanRT& vulkanRT, size_t accelerationStructureSize, VkAccelerationStructureTypeKHR type)
{
    auto& memoryManager = vulkan.GetMemoryManager();

    // Make the acceleration structure buffer.
    auto accelerationStructureBuffer = memoryManager.CreateBuffer(accelerationStructureSize, BufferUsageFlags::AccelerationStructure, MemoryUsage::GpuExclusive);
    if (!accelerationStructureBuffer)
    {
        return false;
    }

    VkAccelerationStructureCreateInfoKHR asCreateInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    asCreateInfo.buffer = accelerationStructureBuffer.GetVkBuffer();
    asCreateInfo.offset = 0;
    asCreateInfo.size = accelerationStructureSize;
    asCreateInfo.type = type;

    auto accelerationStructure = vulkanRT.vkCreateAccelerationStructureKHR(asCreateInfo);
    if (!accelerationStructure)
    {
        memoryManager.Destroy(std::move(accelerationStructureBuffer));
        return false;
    }
    m_accelerationStructure = accelerationStructure;
    m_accelerationStructureBuffer = std::move(accelerationStructureBuffer);
    return true;
}

void AccelerationStructure<Vulkan>::CloneCmd(VulkanRT& vulkanRT, VkCommandBuffer cmdBuffer, const AccelerationStructure<Vulkan>& src) const
{
    assert(src.m_accelerationStructure != VK_NULL_HANDLE);
    assert(m_accelerationStructure != VK_NULL_HANDLE);

    VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
    copyInfo.src = src.m_accelerationStructure;
    copyInfo.dst = m_accelerationStructure;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR;
    vulkanRT.vkCmdCopyAccelerationStructureKHR(cmdBuffer, &copyInfo);
}

void AccelerationStructure<Vulkan>::Destroy(Vulkan& vulkan, VulkanRT& vulkanRT)
{
    vulkanRT.vkDestroyAccelerationStructureKHR(m_accelerationStructure);
    m_accelerationStructure = VK_NULL_HANDLE;
    vulkan.GetMemoryManager().Destroy(std::move(m_accelerationStructureBuffer));
}


bool AccelerationStructureScratch::Create(Vulkan& vulkan, size_t scratchBufferSize)
{
    auto& memoryManager = vulkan.GetMemoryManager();
    m_scratchBuffer = memoryManager.CreateBuffer(scratchBufferSize, BufferUsageFlags::Storage | BufferUsageFlags::ShaderDeviceAddress, MemoryUsage::GpuExclusive);
    if (m_scratchBuffer)
    {
        m_scratchBufferDeviceAddress = memoryManager.GetBufferDeviceAddress(m_scratchBuffer);
        return true;
    }
    else
    {
        m_scratchBufferDeviceAddress = 0;
        return false;
    }
}

void AccelerationStructureScratch::Destroy(Vulkan& vulkan)
{
    m_scratchBufferDeviceAddress = 0;
    vulkan.GetMemoryManager().Destroy(std::move(m_scratchBuffer));
}


bool AccelerationStructureUpdateable::Create(Vulkan& vulkan, VulkanRT& vulkanRT, VkBuildAccelerationStructureFlagsKHR buildFlags, UpdateMode updateMode, const std::span<VkAccelerationStructureGeometryKHR> geometries, const std::span<uint32_t> maxPrimitiveCounts)
{
    assert(updateMode == UpdateMode::NotUpdatable || (buildFlags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR) != 0); // check the build flags are compatible with the updateMode
    m_buildFlags = buildFlags;

    VkAccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    asBuildGeometryInfo.type = m_type;
    asBuildGeometryInfo.flags = buildFlags;
    asBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    asBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    asBuildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
    asBuildGeometryInfo.geometryCount = (uint32_t)geometries.size();
    asBuildGeometryInfo.pGeometries = geometries.data();
    assert(maxPrimitiveCounts.size() == geometries.size());

    // Get the buffer sizes needed for the requested AS
    VkAccelerationStructureBuildSizesInfoKHR asSizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vulkanRT.vkGetAccelerationStructureBuildSizesKHR(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildGeometryInfo, maxPrimitiveCounts.data(), &asSizeInfo);

    // Create the acceleration buffers and their storage
    if (!m_target.Create(vulkan, vulkanRT, asSizeInfo.accelerationStructureSize, m_type))
        return false;
    if (updateMode == UpdateMode::PingPong)
    {
        if (!m_source.Create(vulkan, vulkanRT, asSizeInfo.accelerationStructureSize, m_type))
        {
            m_target.Destroy(vulkan, vulkanRT);
            return false;
        }
    }

    m_buildScratchSize = asSizeInfo.buildScratchSize;
    m_updateScratchSize = asSizeInfo.updateScratchSize;

    return true;
}


void AccelerationStructureUpdateable::Destroy(Vulkan& vulkan, VulkanRT& vulkanRT)
{
    m_target.Destroy(vulkan, vulkanRT);
    m_source.Destroy(vulkan, vulkanRT);
}


void AccelerationStructureUpdateable::BuildAccelerationStructuresCmd(VulkanRT& vulkanRT, VkCommandBuffer commandBuffer, const std::span<VkAccelerationStructureGeometryKHR> geometries, const std::span<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfo, const AccelerationStructureScratch& scratchBuffer) const
{
    VkAccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    asBuildGeometryInfo.type = m_type;
    asBuildGeometryInfo.flags = m_buildFlags;
    asBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    asBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;  // not needed for call to vkCmdBuildAccelerationStructuresKHR
    asBuildGeometryInfo.dstAccelerationStructure = m_target.GetVkAccelerationStructure();
    asBuildGeometryInfo.geometryCount = (uint32_t) geometries.size();
    asBuildGeometryInfo.pGeometries = geometries.data();
    asBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.GetDeviceAddress();

    VkAccelerationStructureBuildRangeInfoKHR* pAsBuildRangeInfo = asBuildRangeInfo.data();
    assert(asBuildRangeInfo.size() == 1);
    vulkanRT.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &asBuildGeometryInfo, &pAsBuildRangeInfo);

    if (m_source)
    {
        // Copy the built AS to the update AS

        // Barrier to ensure build has written the Acceleration Structure before cloning it.
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

        m_source.CloneCmd(vulkanRT, commandBuffer, m_target);
    }
}

void AccelerationStructureUpdateable::PrepareUpdateAccelerationStructuresCmd(VkAccelerationStructureBuildGeometryInfoKHR& asBuildGeometryInfo, const std::span<VkAccelerationStructureGeometryKHR> geometries, const AccelerationStructureScratch& scratchBuffer) const
{
    assert( (m_buildFlags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR) != 0 );
    VkAccelerationStructureKHR source = m_source ? m_source.GetVkAccelerationStructure() : m_target.GetVkAccelerationStructure();
    asBuildGeometryInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = m_type,
        .flags = m_buildFlags,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
        .srcAccelerationStructure = source,
        .dstAccelerationStructure = m_target.GetVkAccelerationStructure(),
        .geometryCount = (uint32_t) geometries.size(),
        .pGeometries = geometries.data(),
        .scratchData = {.deviceAddress = scratchBuffer.GetDeviceAddress() }
    };
}

void AccelerationStructureUpdateable::UpdateAccelerationStructuresCmd(VulkanRT& vulkanRT, VkCommandBuffer commandBuffer, const std::span<VkAccelerationStructureGeometryKHR> geometries, const std::span<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfo, const AccelerationStructureScratch& scratchBuffer) const
{
    VkAccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo;
    PrepareUpdateAccelerationStructuresCmd(asBuildGeometryInfo, geometries, scratchBuffer);

    VkAccelerationStructureBuildRangeInfoKHR* pAsBuildRangeInfo = asBuildRangeInfo.data();
    assert(asBuildRangeInfo.size() == 1);
    vulkanRT.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &asBuildGeometryInfo, &pAsBuildRangeInfo);

    if (m_source)
    {
        // Copy the built AS to the update AS (if we are not building m_target in-place; ie no m_source)

        // Barrier to ensure build has written the Acceleration Structure before cloning it.
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

        m_source.CloneCmd(vulkanRT, commandBuffer, m_target);
    }
}
