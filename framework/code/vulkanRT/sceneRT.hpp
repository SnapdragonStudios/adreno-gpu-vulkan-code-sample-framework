//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

//
// Container for a top level acceleration structure describing a ray-tracable 'scene'
//
#include "meshObjectRT.hpp"
#include "accelerationInstanceBufferObject.hpp"
#include <deque>
#include <glm/mat3x4.hpp>
#include <unordered_map>

// Forward declarations
class Vulkan;
class MeshObjectRT;

class SceneRTBase
{
    SceneRTBase( const SceneRTBase& ) = delete;
    SceneRTBase& operator=( const SceneRTBase& ) = delete;
public:
    SceneRTBase();
    virtual ~SceneRTBase() {}

    // Add the given mesh to the scene, returns id
    virtual uint64_t AddObject( MeshObjectRT object ) { assert( 0 ); return 0; }

    // Add a new instance of a given mesh (id).  Call Update to take all the new instances and replace the existing set with the 'new' set.
    virtual void AddInstance(uint64_t id, glm::mat3x4 transform) { assert( 0 ); }

    // Set to remove all instances on next call to Update.  Clears any prior 'AddInstance' calls.
    virtual void RemoveAllInstances() { assert( 0 ); }
};

// Class
class SceneRT : public SceneRTBase, public AccelerationStructureUpdateable
{
    SceneRT( const SceneRT& ) = delete;
    SceneRT& operator=( const SceneRT& ) = delete;
public:
    SceneRT( Vulkan& vulkan, VulkanRT& vulkanRT );
    SceneRT( SceneRT&&) noexcept;
    ~SceneRT();

    // Add the given mesh to the scene, returns id
    virtual uint64_t AddObject(MeshObjectRT object) override;

    /// @return pointer to MeshObjectRT which was added witht he given id.  Returns nullptr if not found.
    const MeshObjectRT* FindObject(uint64_t id) const;

    // Add a new instance of a given mesh (id).  Call Update to take all the new instances and replace the existing set with the 'new' set.
    virtual void AddInstance(uint64_t id, glm::mat3x4 transform) override;

    // Set to remove all instances on next call to Update.  Clears any prior 'AddInstance' calls.
    virtual void RemoveAllInstances() override;

    // Get the number of 'current' instances.
    size_t GetNumInstances() const { return m_instances.size(); }

    // Update the acceleration structure (potentially replacing the 'instances' with 'newinstances'
    void Update(bool forceRegeneration);

    // Issue the commands to do the gpu side of the scene update (actually update or rebuild the Acceleration structure)
    void UpdateAS(VkCommandBuffer cmdBuffer, bool forceUpdate = false );

    typedef std::pair<uint64_t, glm::mat3x4>           tIdAndTransformPair;
    const std::vector<tIdAndTransformPair>& GetInstances() const { return m_instances; }

protected:
    typedef std::unordered_map<uint64_t, MeshObjectRT> tMeshObjectUnorderedMap;

    // Build the initial acceleration (and scratch) buffer.
    bool CreateAccelerationStructure( UpdateMode updateMode, size_t minSize);

protected:
    Vulkan&                             m_vulkan;
    VulkanRT&                           m_vulkanRT;

    tMeshObjectUnorderedMap             m_newBottomLevelObjects;   // Objects we want to add in to the acceleration structure
    tMeshObjectUnorderedMap             m_bottomLevelObjects;      // Bottom level objects used in the current top level acceleration structure

    std::vector<tIdAndTransformPair>    m_newInstances;
    std::vector<tIdAndTransformPair>    m_instances;

    size_t                              m_instanceBufferNumUsed = 0;
    AccelerationInstanceBufferObject    m_instanceBuffer;
    uint64_t                            m_instanceBufferDeviceAddress = 0;
    VkBuildAccelerationStructureFlagsKHR m_accelerationBuildFlags = 0;
    AccelerationStructureScratch        m_accelerationBuildScratch;

    bool                                m_pendingInstancesUpdate  = false;
    bool                                m_accelerationStructurePendingGpuRebuild = false;
};

#include "mesh/octree.hpp"

class SceneRTCullable : public SceneRT
{
public:
    SceneRTCullable(Vulkan& vulkan, VulkanRT& vulkanRT);

    typedef uint64_t tModelId;

    /// Add an instance of a given mesh (id)
    virtual void AddInstance(tModelId modelId, glm::mat3x4 transform) override;

    size_t GetNumKnownInstances() const { return m_knownInstances.size(); }

    /// Build the acceleration (and scratch) buffer.
    /// Only call this if we want to be able to trace this scene (rather than using this as a container for the instances and octree that are traced via a SceneRTCulled).
    bool CreateAccelerationStructure();

    friend class SceneRTCulled;

protected:
    typedef uint32_t tKnownInstancesIndex;
    
    struct Instance
    {
        tModelId modelId;
        glm::mat3x4 transform;
    };

    std::vector<Instance>                   m_knownInstances;
    Octree<tKnownInstancesIndex, 4>         m_Octree;
};


class SceneRTCulled : public SceneRT
{
public:
    SceneRTCulled(Vulkan& vulkan, VulkanRT& vulkanRT);

    // Set/Get the forced regen (regenerates and rebuilds the top level AS on every update, not just when there are changes.  Be aware that setting this could greatly slow down the GPU.
    void SetForceRegenerateAccelerationStructure(bool b) { m_forceRegenerateAccelerationStructure = b; }
    bool GetForceRegenerateAccelerationStructure() const { return m_forceRegenerateAccelerationStructure; }

    // Update the Scene.  Templated to allow user defined tests against the Octree!
    template<typename T_TEST>
    void Update(const SceneRTCullable& scene/*scene we will generate a RT Acceleration Structure from*/, const T_TEST& cullTest);

    // Build the initial acceleration (and scratch) buffer. (makes base class function public)
    bool CreateAccelerationStructure(UpdateMode updateMode, size_t minSize) { return SceneRT::CreateAccelerationStructure( updateMode, minSize);  }

protected:
    void PostQueryUpdate(const SceneRTCullable& scene, bool regenerateInstances);

protected:
    typedef SceneRTCullable::tKnownInstancesIndex tKnownInstancesIndex;
    typedef SceneRTCullable::tModelId             tKnownModelIndex;

    std::vector<tKnownInstancesIndex>       m_visibleInstanceIndices; // stores indices of all the instances currently visible (sized to contain all known instances, only valid for first m_numVisibleInstanceIndices elements)
    size_t                                  m_numVisibleInstanceIndices = 0;
    bool                                    m_forceRegenerateAccelerationStructure;
    bool                                    m_forceUpdateAccelerationStructure;
};


template<typename T_TEST>
void SceneRTCulled::Update(const SceneRTCullable& scene/*scene we will generate a RT Acceleration Structure from*/, const T_TEST& testFn)
{
    // Query the octree and compare against the list of visible instances we had last frame.
    // Assumes (demands) that the octree traverses in the same order every frame.

    if( m_visibleInstanceIndices.size() != scene.GetNumKnownInstances() )
    {
        // resize to the maximum possible instances (and clear the old data (count) since we can assume a resize means it is no longer valid)
        m_visibleInstanceIndices.resize( scene.m_knownInstances.size() );
        m_numVisibleInstanceIndices = 0;
    }

    auto lastVisibleIt = m_visibleInstanceIndices.begin();
    auto lastVisibleEndIt = lastVisibleIt + m_numVisibleInstanceIndices;
    bool visibilityChanged = false;

    scene.m_Octree.Query( testFn, [&visibilityChanged, &lastVisibleIt, &lastVisibleEndIt]( const tKnownInstancesIndex& visibleIdx ) {
        // Query found an instance.
        if( !visibilityChanged && *lastVisibleIt == visibleIdx && lastVisibleIt < lastVisibleEndIt )
        {
            // our octree query is still matching the last frame.
        }
        else
        {
            // not matching last frame.
            visibilityChanged = true;
            *lastVisibleIt = visibleIdx;
        }
        ++lastVisibleIt;
    } );
    size_t numVisible = lastVisibleIt - m_visibleInstanceIndices.begin();
    if( m_numVisibleInstanceIndices != numVisible )
    {
        m_numVisibleInstanceIndices = numVisible;
        visibilityChanged = true;
    }
    assert( lastVisibleIt <= m_visibleInstanceIndices.end() );

    PostQueryUpdate(scene, visibilityChanged);
}

