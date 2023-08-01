//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once
#include "vulkan/vulkan.hpp"
#include "memory/memoryMapped.hpp"

class VulkanRT;

/// @brief Base class for anything using a Ray Tracing Acceleration structure.
/// Contains the Acceleration structure and it's memory buffer
class AccelerationStructure
{
    AccelerationStructure& operator=(const AccelerationStructure&) = delete;
    AccelerationStructure(const AccelerationStructure&) = delete;

public:
    AccelerationStructure() : m_accelerationStructure(VK_NULL_HANDLE) {}
    AccelerationStructure(AccelerationStructure&& other) noexcept
        : m_accelerationStructureBuffer(std::move(other.m_accelerationStructureBuffer))
    {
        m_accelerationStructure = other.m_accelerationStructure;
        other.m_accelerationStructure = VK_NULL_HANDLE;
    }

    /// True if the acceleration structure is 'valid' (may not be bound to any data but has been created)
    operator bool() const { return m_accelerationStructure != VK_NULL_HANDLE; }

    /// Create the Acceleration Structure (and its buffer)
    bool Create(Vulkan& vulkan, VulkanRT& vulkanRT, size_t accelerationStructureSize, VkAccelerationStructureTypeKHR type);

    /// Destroy the Acceleration structure (and its buffer) if they have been created/allocated (otherwise do nothing)
    virtual void Destroy(Vulkan& vulkan, VulkanRT& vulkanRT);

    /// Add the GPU commands to clone the @src accleleration structure into this one.
    /// Requires/expects that the acceleration structure is Created and is sized appropriately.
    void CloneCmd(VulkanRT& vulkanRT, VkCommandBuffer cmdBuffer, const AccelerationStructure& src) const;

    /// @return Vulkan Acceletation Structure owned by this class
    VkAccelerationStructureKHR GetVkAccelerationStructure()const { return m_accelerationStructure; }

private:
    VkAccelerationStructureKHR          m_accelerationStructure;
    MemoryAllocatedBuffer<Vulkan, VkBuffer>  m_accelerationStructureBuffer;
};


class AccelerationStructureScratch
{
    AccelerationStructureScratch& operator=(const AccelerationStructureScratch&) = delete;
    AccelerationStructureScratch(const AccelerationStructureScratch&) = delete;

public:
    AccelerationStructureScratch() : m_scratchBufferDeviceAddress(0) {}
    AccelerationStructureScratch(AccelerationStructureScratch&& other) noexcept
        : m_scratchBuffer(std::move(other.m_scratchBuffer))
        , m_scratchBufferDeviceAddress(other.m_scratchBufferDeviceAddress)
    {
        other.m_scratchBufferDeviceAddress = 0;
    }

    bool Create(Vulkan& vulkan, size_t scratchBufferSize);
    void Destroy(Vulkan& vulkan);

    uint64_t GetDeviceAddress() const { return m_scratchBufferDeviceAddress; }

private:
    MemoryAllocatedBuffer<Vulkan, VkBuffer>  m_scratchBuffer;
    uint64_t                            m_scratchBufferDeviceAddress;
};


/// @brief Base class for an Updatable Ray Tracing Acceleration structure.
/// Always contains a 'target' acceleration structure which we ray trace against and either update in-place or update from 'source'.
/// Creation of the source structure is optional (if present it is used as the source for update and ping-ponged with the target).
class AccelerationStructureUpdateable
{
    AccelerationStructureUpdateable& operator=(const AccelerationStructureUpdateable&) = delete;
    AccelerationStructureUpdateable(const AccelerationStructureUpdateable&) = delete;

public:
    AccelerationStructureUpdateable(const VkAccelerationStructureTypeKHR type) : m_type(type) {}
    AccelerationStructureUpdateable(AccelerationStructureUpdateable&& other) noexcept
        : m_type(other.m_type)
        , m_buildFlags(other.m_buildFlags)
        , m_updateMode(other.m_updateMode)
        , m_buildScratchSize(other.m_buildScratchSize)
        , m_updateScratchSize(other.m_updateScratchSize)
        , m_source(std::move(other.m_source))
        , m_target(std::move(other.m_target))
    {
        other.m_buildFlags = 0;
        other.m_buildScratchSize = 0;
        other.m_updateScratchSize = 0;
    }

    /// True if the primary acceleration structure is 'valid' (may not be bound to any data but has been created)
    operator bool() const { return !!m_source; }

    enum class UpdateMode {
        NotUpdatable,
        InPlace,
        PingPong
    };

    /// @brief Create the Acceleration Structure(s) (and their buffers).
    /// @param buildFlags build flags stored for use by BuildAccelerationStructuresCmd (Create does not build the AS).  If VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR is set AND updateInPlace==true the m_source acceleration structure is also created.
    /// @param updateMode determines if updates use m_source to update into m_destination (PingPong) or if m_destination is updated in place (InPlace)
    /// @return true if successfully created
    bool Create(Vulkan& vulkan, VulkanRT& vulkanRT, VkBuildAccelerationStructureFlagsKHR buildFlags, UpdateMode updateMode, const std::span<VkAccelerationStructureGeometryKHR> geometries, const std::span<uint32_t> maxPrimitiveCounts);

    /// Destroy the Acceleration structure(s) (and their buffers) if they have been created/allocated (otherwise do nothing)
    virtual void Destroy(Vulkan& vulkan, VulkanRT& vulkanRT);

    /// @return Vulkan Acceleration Structure for Ray Querying/Tracing
    VkAccelerationStructureKHR GetVkAccelerationStructure()const { return m_target.GetVkAccelerationStructure(); }

    VkDeviceSize GetBuildScratchSize() const { return m_buildScratchSize; }
    VkDeviceSize GetUpdateScratchSize() const { return m_updateScratchSize; }

    /// @brief Output the Vulkan commands to build the m_target acceleration structure from the given geometry.  Will clone that built data to m_source if it is setup.
    void BuildAccelerationStructuresCmd(VulkanRT& vulkanRT, VkCommandBuffer commandBuffer, const std::span<VkAccelerationStructureGeometryKHR> geometries, const std::span<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfo, const AccelerationStructureScratch& scratchBuffer) const;

    /// @brief Output the Vulkan commands to update the m_target acceleration structure, based on m_source, from the given geometry.
    void UpdateAccelerationStructuresCmd(VulkanRT& vulkanRT, VkCommandBuffer commandBuffer, const std::span<VkAccelerationStructureGeometryKHR> geometries, const std::span<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfo, const AccelerationStructureScratch& scratchBuffer) const;

    /// @brief Output the Vulkan structure to update the m_target acceleration structure, based on m_source, from the given geometry.  Intended for batching together several of these updates in a single Vulkan command.
    void PrepareUpdateAccelerationStructuresCmd( VkAccelerationStructureBuildGeometryInfoKHR& asBuildGeometryInfo, const std::span<VkAccelerationStructureGeometryKHR> geometries, const AccelerationStructureScratch& scratchBuffer ) const;

private:
    const VkAccelerationStructureTypeKHR m_type;
    VkBuildAccelerationStructureFlagsKHR m_buildFlags = 0;
    UpdateMode m_updateMode = UpdateMode::NotUpdatable;
    size_t m_buildScratchSize = 0;
    size_t m_updateScratchSize = 0;
    AccelerationStructure m_source;
    AccelerationStructure m_target;
};
