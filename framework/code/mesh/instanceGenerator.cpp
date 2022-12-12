//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "instanceGenerator.hpp"
#include "mesh/meshObjectIntermediate.hpp"
#include "system/crc32c.hpp"
#include <glm/gtx/norm.hpp>
#define EIGEN_INITIALIZE_MATRICES_BY_ZERO
#define EIGEN_MPL2_ONLY
#include <eigen/Eigen/Dense>
#include <map>
#include <algorithm>

// Calculate the 'centroid' of the object mesh.
static glm::vec3 ComputeMeshCenter( const tcb::span<MeshObjectIntermediate::FatVertex> vertices )
{
    glm::highp_dvec3 center(0.0);   // calculate in 'doubles'
    {
        int i = 0;
        for (; i < vertices.size() && i < 32; ++i)
        {
            const auto& v = vertices[i];
            center += glm::highp_dvec3(v.position[0], v.position[1], v.position[2]);
        }
        center /= i;
    }
    return center;
}

static void TransformToCenter( const tcb::span<MeshObjectIntermediate::FatVertex> vertices, const glm::vec3 center )
{
    for (auto& v: vertices)
    {
        v.position[0] -= center.x;
        v.position[1] -= center.y;
        v.position[2] -= center.z;
    }
}

static glm::mat4x4 ComputeTransformationBetweenVertexPositions( const tcb::span<const MeshObjectIntermediate::FatVertex>& verticesFrom,
                                                                const tcb::span<const MeshObjectIntermediate::FatVertex>& verticesTo,
                                                                const glm::highp_dvec3 verticesFromCenter,
                                                                const glm::highp_dvec3 verticesToCenter )
{
    // Dot product the two sets of positions!
    Eigen::Matrix3d m33;
    for (int i = 0; i < verticesFrom.size() && i < 32; ++i)
    {
        glm::highp_dvec3 p0 = glm::highp_dvec3(verticesFrom[i].position[0], verticesFrom[i].position[1], verticesFrom[i].position[2]) - verticesFromCenter;
        glm::highp_dvec3 p1 = glm::highp_dvec3(verticesTo[i].position[0], verticesTo[i].position[1], verticesTo[i].position[2]) - verticesToCenter;

        m33(0, 0) += p0[0] * p1[0];
        m33(0, 1) += p0[0] * p1[1];
        m33(0, 2) += p0[0] * p1[2];
        m33(1, 0) += p0[1] * p1[0];
        m33(1, 1) += p0[1] * p1[1];
        m33(1, 2) += p0[1] * p1[2];
        m33(2, 0) += p0[2] * p1[0];
        m33(2, 1) += p0[2] * p1[1];
        m33(2, 2) += p0[2] * p1[2];
    }

    // Decompose the 3x3 
    Eigen::JacobiSVD<Eigen::Matrix3d> svd;
    svd.compute(m33, Eigen::ComputeFullU | Eigen::ComputeFullV);
    
    auto V = svd.matrixV();
    auto UT = svd.matrixU().transpose();
    Eigen::MatrixXd rotation = V * UT;

    // Make sure the rotation is not mirrored (can happen when using SVD this way).
    if (rotation.determinant() < 0.0)
    {
        // Rotation mirrored, flip last column of V and regenerate the rotation matrix.
        V(0,2) *= -1.0;
        V(1,2) *= -1.0;
        V(2,2) *= -1.0;
        rotation = V * UT;
    }
    // Rotation is correct now (assuming the 2 sets of vertices are truely just translated and rotated versions of each other).
    // Compose the final transform...
    auto transform = Eigen::Translation3d(verticesToCenter.x, verticesToCenter.y, verticesToCenter.z) * rotation * Eigen::Translation3d(-verticesFromCenter.x, -verticesFromCenter.y, -verticesFromCenter.z);
    //auto transform = Eigen::Translation3d(-verticesFromCenter.x, -verticesFromCenter.y, -verticesFromCenter.z) * rotation * Eigen::Translation3d(verticesToCenter.x, verticesToCenter.y, verticesToCenter.z);
    glm::mat4x4 ret( *(glm::dmat4x4*)transform.matrix().data() );
    static_assert(sizeof(glm::dmat4x4) == sizeof(double) * 4 * 4);
    return ret;
}

std::vector<MeshInstance> MeshInstanceGenerator::NullFindInstances(std::vector<MeshObjectIntermediate> objects)
{
    std::vector<MeshInstance> out;
    for (auto& object : objects)
    {
        glm::mat3x4 instanceTransform = glm::transpose(object.m_Transform);
        object.m_Transform = glm::identity<glm::mat4>();
        out.push_back({ std::move(object), {{instanceTransform, object.m_NodeId}} });
    }
    return out;
}

std::vector<MeshInstance> MeshInstanceGenerator::FindInstances(std::vector<MeshObjectIntermediate> objects)
{
    std::multimap<uint32_t, MeshObjectIntermediate> matchingSets;

    // Go through and match based on a CRC (of UV positions and materials).
    // Normals and postions are not a reliable indicator as they will be rotated/translated differently for matching instances. 
    // Move the objects in to this map!
    for (auto& object : objects)
    {
        size_t bufferSize = object.m_VertexBuffer.size();
        uint32_t crc = crc32c(0, { (uint8_t*)&bufferSize, sizeof(bufferSize) });
        for (const auto& vert : object.m_VertexBuffer)
            crc = crc32c(crc, { (uint8_t*)vert.uv0, sizeof(vert.uv0) });
        for (const MeshObjectIntermediate::MaterialDef& material : object.m_Materials)
            crc = crc32c(crc, material.diffuseFilename);
        matchingSets.emplace( crc, std::move(object) );
    }

    // Find how many unique crc values there were (gives a good start for number of truely unique mesh instances).
    uint32_t lastCrc = 0xffffffff;
    uint32_t numUniqueCrc = 0;
    for (const auto& [crc, object] : matchingSets)
    {
        if (crc == lastCrc)
            ++numUniqueCrc;
        lastCrc = crc;
    }
    std::vector<MeshInstance> instances;
    instances.reserve(numUniqueCrc);

    // Go through the 'unique' sets and determine the transform for each instance (to map it to the position of the 'original').
    // Build a list of unique MeshObjects and their instances (with transforms).
    // We repeat until 'matchingSets' has been emptied.  Worse case becomes N^2, where all the meshes have identical CRC values but their meshes dont match.
    while (!matchingSets.empty())
    {
        uint32_t setCrc = 0xffffffff;
        glm::vec3 setFirstCenter{};
        for (auto it = matchingSets.begin(); it != matchingSets.end(); )
        {
            const auto crc = it->first;
            auto& object = it->second;

            glm::vec3 center = ComputeMeshCenter(object.m_VertexBuffer);

            if (crc != setCrc)
            {
                // First item in a new set of matches.
                // Add it as a new set of 'instances' and remove from the 'matchingSets'.
                setCrc = crc;
                setFirstCenter = center;

                TransformToCenter( object.m_VertexBuffer, center );

                glm::mat4 m = glm::identity<glm::mat4>();
                m[3].x = center.x;
                m[3].y = center.y;
                m[3].z = center.z;
                m = object.m_Transform * m;

                instances.push_back({ std::move(object), {{glm::transpose(m), -1}} });
                it = matchingSets.erase(it);
            }
            else
            {
                // First 'duplicate' in the set.

                //
                // Use SVD to determine the rotation between the 2 sets of vertices.
                //
                const MeshObjectIntermediate& setFirstObject = instances.rbegin()->mesh;
                auto transform = ComputeTransformationBetweenVertexPositions(setFirstObject.m_VertexBuffer, object.m_VertexBuffer, glm::vec3(0.0f), center);

                //
                // Sanity check that the transform really does map between the 2 sets of vertices.
                // This will fail if the mesh positions are not truely identical (outside of translation/rotation).
                //
                bool verificationFailed = false;
                for (int i = 0; i < object.m_VertexBuffer.size(); ++i)
                {
                    const glm::vec3 p0 = glm::vec3(setFirstObject.m_VertexBuffer[i].position[0], setFirstObject.m_VertexBuffer[i].position[1], setFirstObject.m_VertexBuffer[i].position[2]);
                    const glm::vec3 ptest2 = transform * glm::vec4(p0, 1.0f);
                    const glm::vec3 p1 = glm::vec3(object.m_VertexBuffer[i].position[0], object.m_VertexBuffer[i].position[1], object.m_VertexBuffer[i].position[2]);
                    auto pdist2 = glm::distance2(p1, ptest2);
                    if (pdist2 > 1.0f)
                    {
                        // assume too much error!
                        verificationFailed = true;
                        break;
                    }
                }
                if (verificationFailed)
                {
                    // Transform failed to transform vertices correctly - assume the meshes aren't matches (either aren't based on each other or are scaled, which we dont currently handle)
                    // Leave this instance in the 'matchSets' list.
                    ++it;
                }
                else
                {
                    // Transform looks good.  Add this as a new instance of the current instances set.
                    glm::mat4 m = object.m_Transform * transform;
                    instances.rbegin()->instances.push_back({ glm::transpose(m), -1 });
                    it = matchingSets.erase(it);
                }
            }
        }
    }

    // Clear out the mesh transforms now they have all been applied in to the instance transforms.
    // Also clear the nodeId for the root mesh...
    ///TODO: store m_NodeId for each of the instances and calculate transform that allows the instance node to be animated correctly.
    ///NOTE: we could keep the m_NodeId for any meshes that have a unique instance, at the cost of potential confusion when there is a mix of instanced and non instanced geometry.
    for (auto& instance : instances)
    {
        instance.mesh.m_Transform = glm::identity<glm::mat4>();
        instance.mesh.m_NodeId = -1;
    }

    return instances;
}
