//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <glm/glm.hpp>

const static glm::vec4 sCellOffsets[8] = {
    {-1.f,-1.f, -1.f, 0.f}, {1.f,-1.f,-1.f, 0.f}, {-1.f,1.f,-1.f, 0.f}, {1.f,1.f,-1.f, 0.f},
    {-1.f,-1.f,  1.f, 0.f}, {1.f,-1.f, 1.f, 0.f}, {-1.f,1.f, 1.f, 0.f}, {1.f,1.f, 1.f, 0.f}
};

class OctreeBase
{
public:
    enum class eQueryResult
    {
        Inside, Outside, Partial
    };
};


/// Simple octree class.
/// Designed for culling of (mostly) static geometry.
/// 
/// Should give very good query and traversal performance (nodes are stored linearly in memory and ordered such that every node under a single octree node can be traversed linearly).
/// 
/// Has APALLING add/insert performance, potentially an add will cause all data in the octree to be moved.
/// Could be improved 
///
/// @tparam T_OBJECT object contained in the octree.  Keep small as these will be copied (a lot) on insertion.
/// @tparam T_MAXDEPTH maximum depth (levels of nodes).  If 5 or less the octree will use less memory (16bit node indices)
/// @ingroup Mesh
template<typename T_OBJECT, uint32_t T_MAXDEPTH>
class Octree : public OctreeBase
{
    typedef std::integral_constant<uint32_t, T_MAXDEPTH> tDepthVal;
    typedef uint32_t tObjectIdx;
    typedef typename std::conditional<T_MAXDEPTH <= 5 , uint16_t, uint32_t>::type tNodeCount;   // 5 deep is 37448 max nodes (1 + 8 + 64 + 64*8 + 64*64 + 64*64*8)

    /// Data describing contents of 8 cells (2x2x2 grid) making up this Octree Node
    struct Node
    {
        std::array<tObjectIdx, 8> ChildObjectCountTotal;///< count of objects for all nodes BELOW these 8 cells. TOTAL (eg accumulates from prior cells in the array).  The number of objects sat in this cell (ie not at a lower level) is the count from the parent Node - ObjectCountTotal[7]
        std::array<tNodeCount, 8> ChildNodeCountTotal;  ///< number of child nodes (including grandchildren etc) BELOW each of the cells in this Node level.  TOTAL (eg ChildNodeCountTotal[1] is the count of nodes below cell 1 + count of nodes below cell 0, ChildNodeCountTotal[7] is the total number of nodes below this entire octree node)
    };

public:
    typedef T_OBJECT tObject;

    /// Constructor
    /// @param center Center position of the octree (nodes will split in 3 dimensions around this center)
    /// @param octreeSize Dimensions of octree (not halfsize)
    /// @param maxObjects Maximum number of objects octree can contain
    Octree( const glm::vec3 center, const glm::vec3 octreeSize, uint32_t maxObjects ) : m_Center( center, 1.0f ), m_HalfSize( octreeSize*0.5f, 0.0f ), m_Objects(), m_Nodes( 1, Node{} )
    {
        m_Objects.reserve( maxObjects );
    }

    /// Add object to the octree.
    /// @note SLOW
    void AddObject( const glm::vec4& objectPosition, const glm::vec4& objectSize/*NOT a half size*/, T_OBJECT&& object );

    /// Query against this octree and output all contained objects. 
    template<typename T_TEST, typename T_OUTPUT>
    void Query( const T_TEST& testFn, T_OUTPUT&& outputFn ) const;

private:

    // Gives the cell 'index' (octant) based upon the sign of the 3 axis.  Returns 0-7 (inclusive).
    uint8_t CalcCellIndex( const glm::vec4& pos )
    {
        uint8_t cellIndex = 0;
        if( pos.x >= 0.0f )
        {
            cellIndex |= 1;
        }
        if( pos.y >= 0.0f )
        {
            cellIndex |= 2;
        }
        if( pos.z >= 0.0f )
        {
            cellIndex |= 4;
        }
        return cellIndex;
    }

    // Internal (recurive) implementations
    uint32_t AddObject( uint32_t nodeIdx, uint32_t objectIdx, int depth, const glm::vec4& relativePosition, const glm::vec4& scaledObjectSize/* doubled as we go down each level!*/, T_OBJECT&& object );

    template<typename T_TEST, typename T_OUTPUT>
    void Query( const T_TEST& testFn, T_OUTPUT&& outputFn, uint32_t nodeIdx, uint32_t objectIdx, uint32_t objectEndIdx/*index of end of m_Object span for this node and all the nodes below*/, glm::vec4 center, glm::vec4 halfSize ) const;

private:
    std::vector<Node>       m_Nodes;
    std::vector<T_OBJECT>   m_Objects;

    glm::vec4               m_Center;
    glm::vec4               m_HalfSize;     // size (width, height, depth) of the octree (halved)
};


template<typename T_OBJECT, uint32_t T_MAXDEPTH>
void Octree<T_OBJECT, T_MAXDEPTH>::AddObject( const glm::vec4& objectPosition, const glm::vec4& objectSize/*NOT a half size*/, T_OBJECT && object )
{
    uint32_t nodeIdx = 0;

    glm::vec4 pos = objectPosition - m_Center;

    if( objectSize.x < m_HalfSize.x && objectSize.y < m_HalfSize.y && objectSize.z < m_HalfSize.z )
    {
        // Add object into one of the child cells in the octree.
        AddObject( nodeIdx, 0, 0, pos, objectSize * 2.0f, std::move( object ) );
    }
    else
    {
        // Object too big to go in to any quadtree cells.  Just add to the top node (ie very end of the m_Object array).
        // Will always be returned by a query!
        m_Objects.push_back( std::move(object) );
    }
}

template<typename T_OBJECT, uint32_t T_MAXDEPTH>
uint32_t Octree<T_OBJECT, T_MAXDEPTH>::AddObject(uint32_t nodeIdx, uint32_t objectIdx, int depth, const glm::vec4& relativePosition, const glm::vec4& scaledObjectSize/* doubled as we go down each level!*/, T_OBJECT && object )
{
    uint8_t cellIndex = CalcCellIndex(relativePosition);

    uint32_t numNewNodes = 0;

    Node* pNode = &m_Nodes[nodeIdx];

    if( depth < T_MAXDEPTH && (scaledObjectSize.x < m_HalfSize.x && scaledObjectSize.y < m_HalfSize.y && scaledObjectSize.z < m_HalfSize.z) )
    {
        // Not at the maximum depth and object is smaller than half the curent level's size.
        // we want to recurse down a level (smaller cell).

        const tNodeCount childOffset = (cellIndex > 0) ? pNode->ChildNodeCountTotal[cellIndex - 1] : 0;
        const tNodeCount childCount = pNode->ChildNodeCountTotal[cellIndex] - childOffset;
        const tNodeCount childNodeIdx = nodeIdx + 1 + childOffset;
        if( cellIndex > 0 )
        {
            objectIdx += pNode->ChildObjectCountTotal[cellIndex - 1];
        }

        if( childCount == 0 )
        {
            numNewNodes = 1;

            // Add a new child node (previously didnt have one).
            m_Nodes.emplace( m_Nodes.begin() + childNodeIdx, Node{0,0} );
        }

        // Recurse in to the node.
        numNewNodes += AddObject( childNodeIdx, objectIdx, depth + 1,  (2.0f * relativePosition - m_HalfSize * sCellOffsets[cellIndex]), scaledObjectSize * 2.0f, std::move(object) );

        if( numNewNodes > 0 )
        {
            // the m_Nodes vector may have moved, re-grab pNode.
            pNode = &m_Nodes[nodeIdx];

            // Bump the (cumulative) node counts.
            for( uint32_t i = cellIndex; i < 8; ++i )
            {
                pNode->ChildNodeCountTotal[i] += numNewNodes;
            }
        }

        // Increment the object counts to account for the object we just inserted (done at each level as we step back out of the recursion).
        for( int i = cellIndex; i < 8; ++i )
            pNode->ChildObjectCountTotal[i]++;
    }
    else
    {
        // Found the node where this object should live!
        Node& node = m_Nodes[nodeIdx];
        objectIdx += node.ChildObjectCountTotal[cellIndex];
        m_Objects.insert( m_Objects.begin() + objectIdx, std::move(object) );

        // Increment the object counts to account for the object we just inserted.
        for( int i = cellIndex; i < 8; ++i )
            node.ChildObjectCountTotal[i]++;
    }

    return numNewNodes;
}

template<typename T_OBJECT, uint32_t T_MAXDEPTH>
template<typename T_TEST, typename T_OUTPUT>
void Octree<T_OBJECT, T_MAXDEPTH>::Query(const T_TEST& testFn, T_OUTPUT&& outputFn) const
{
    Query( testFn, outputFn, 0, 0, (uint32_t) m_Objects.size(), m_Center, m_HalfSize );
}

template<typename T_OBJECT, uint32_t T_MAXDEPTH>
template<typename T_TEST, typename T_OUTPUT>
void Octree<T_OBJECT, T_MAXDEPTH>::Query(const T_TEST& testFn, T_OUTPUT&& outputFn, uint32_t nodeIdx, uint32_t objectIdx, uint32_t objectEndIdx/*index of end of m_Object span for this node and all the nodes below*/, glm::vec4 center, glm::vec4 halfSize ) const
{
    halfSize *= 0.5f;
    const Node& node = m_Nodes[nodeIdx];

    for( uint32_t cell = 0; cell < 8; ++cell )
    {
        const uint32_t childObjectOffset = (cell>0) ? node.ChildObjectCountTotal[cell-1] : 0;
        uint32_t childObjectIdx = objectIdx + childObjectOffset;
        const uint32_t childObjectEndIdx = objectIdx + node.ChildObjectCountTotal[cell];//end of m_Objects range for the child.  Exclusive.

        if( childObjectIdx == childObjectEndIdx )
        {
            // No need to test anything if there are no objects in this cell.
            continue;
        }

        // Get the center co-ordinates of this cell.
        const auto cellCenter = center + sCellOffsets[cell] * halfSize;

        // Call our user supplied 'test' function for this cell.
        switch( testFn( cellCenter, halfSize ) )
        {
        case eQueryResult::Inside:  // The cell is completely 'inside' the test area.
        {
            // Output everything under this node!
            while( childObjectIdx < childObjectEndIdx )
            {
                outputFn( m_Objects[childObjectIdx] );
                ++childObjectIdx;
            }
            break;
        }
        case eQueryResult::Partial: // The cell is 'partially' inside the test area.
        {
            // Recurse down a level to see if we can reject some of the smaller cells.
            uint32_t childNodeOffset = (cell > 0) ? node.ChildNodeCountTotal[cell - 1] : 0;
            const uint32_t childNodeCount = node.ChildNodeCountTotal[cell] - childNodeOffset;
            const uint32_t childNodeIdx = nodeIdx + 1/*skip the node we are processing*/ + childNodeOffset;

            if( childNodeCount != 0 )
            {
                // Recurse in to this cell's child node.
                Query( testFn, outputFn, nodeIdx + 1 + childNodeOffset, childObjectIdx, childObjectEndIdx, cellCenter, halfSize );
            }
            else
            {
                // No nodes below this.  Output all the objects for this cell.
                while( childObjectIdx < childObjectEndIdx )
                {
                    outputFn( m_Objects[childObjectIdx] );
                    ++childObjectIdx;
                }
            }
            break;
        }
        case eQueryResult::Outside: // The cell is completely outside the test area.
        {
            // Reject everything!
            break;
        }
        }
    }

    // output everything that is at the current node level (ie not at a lower level).
    // These objects are after any objects in child nodes.
    uint32_t childObjectIdx = objectIdx + node.ChildObjectCountTotal[7];
    while( childObjectIdx < objectEndIdx )
    {
        outputFn( m_Objects[childObjectIdx] );
        ++childObjectIdx;
    }
}


/// Helper axis aligned bounding box test functor 
/// Use with Octree to query the octree against a box.
/// @ingroup Mesh
struct BBoxTest
{
    BBoxTest( const glm::vec3& boxCenter, const glm::vec3& boxHalfSize ) : m_boxCenter( boxCenter ), m_boxHalfSize( boxHalfSize ) {
    }

    OctreeBase::eQueryResult operator()( const glm::vec4& center, const glm::vec4& halfSize ) const
    {
        // Expand out the query because we allow objects up to half the cell size in each cell.
        const glm::vec4 querySize = halfSize + 0.5f * halfSize;

        if( ((center.x + querySize.x < m_boxCenter.x - m_boxHalfSize.x) ||
              (center.x - querySize.x > m_boxCenter.x + m_boxHalfSize.x)) ||
            ((center.y + querySize.y < m_boxCenter.y - m_boxHalfSize.y) ||
              (center.y - querySize.y > m_boxCenter.y + m_boxHalfSize.y)) ||
            ((center.z + querySize.z < m_boxCenter.z - m_boxHalfSize.z) ||
              (center.z - querySize.z > m_boxCenter.z + m_boxHalfSize.z)) )
        {
            return OctreeBase::eQueryResult::Outside;
        }
        else
        {
            if( ((center.x + halfSize.x < m_boxCenter.x + m_boxHalfSize.x) &&
                  (center.x - halfSize.x > m_boxCenter.x - m_boxHalfSize.x) &&
                  (center.y + halfSize.y < m_boxCenter.y + m_boxHalfSize.y) &&
                  (center.y - halfSize.y > m_boxCenter.y - m_boxHalfSize.y) &&
                  (center.z + halfSize.z < m_boxCenter.z + m_boxHalfSize.z) &&
                  (center.z - halfSize.z > m_boxCenter.z - m_boxHalfSize.z)) )
            {
                return OctreeBase::eQueryResult::Inside;
            }
            else
            {
                return OctreeBase::eQueryResult::Partial;
            }
        }
    };

    glm::vec3 m_boxCenter;
    glm::vec3 m_boxHalfSize;
};


/// Helper sphere test functor 
/// Use with Octree to query the octree against a sphere.
/// @ingroup Mesh
struct SphereTest
{
    glm::vec3 m_center;
    float m_radius;

    SphereTest( const glm::vec3& center, float radius ) : m_center( center ), m_radius( radius ) {
    }

    OctreeBase::eQueryResult operator()( const glm::vec3& boxCenter, const glm::vec3& boxHalfSize ) const
    {
        // Box (AABB) inside sphere check.
        const glm::vec3 boxMin = boxCenter - boxHalfSize;
        const glm::vec3 boxMax = boxCenter + boxHalfSize;

        glm::vec3 a = m_center - boxMin;
        glm::vec3 b = m_center - boxMax;

        glm::vec3 dmin = glm::min( a, 0.0f ) + glm::max( b, 0.0f );
        dmin *= dmin;
        glm::vec3 dmax = glm::max( a*a, b*b );

        if( dmin.x + dmin.y + dmin.z > m_radius * m_radius )
        {
            return OctreeBase::eQueryResult::Outside;
        }
        if( dmax.x + dmax.y + dmax.z > m_radius * m_radius )
        {
            return OctreeBase::eQueryResult::Partial;
        }
        return OctreeBase::eQueryResult::Inside;
    }

#if 0
    {
        // Sphere inside box check.
        const glm::vec4 boxMin = boxCenter - boxHalfSize;
        const glm::vec4 boxMax = boxCenter + boxHalfSize;

        float overlapDistSummedSq = 0;
        glm::vec4 overlapDist4 = glm::min( m_center - boxMax, 0.0f ) + glm::min( boxMin - m_center, 0.0f );
        overlapDist4 *= overlapDist4;
        float overlapDistSq = overlapDist4.x + overlapDist4.y + overlapDist4.z;
        if( overlapDistSq <= 0.0f )
        {
            return OctreeBase::eQueryResult::Inside;    // sp
        }
        else if (overlapDistSq <= m_radius*m_radius )
        {
            return OctreeBase::eQueryResult::Partial;   // overlapping.
        }
    }
#endif
};


/// View frustum
/// Builds the 
/// @ingroup Mesh
class ViewFrustum
{
public:
    ViewFrustum( const glm::mat4x4& perspective, const glm::mat4x4& view )
    {
        const auto viewT = glm::transpose( perspective * view );

        // Create the frustum planes.  Order is not important for functionality but ordering 'most likely to reject' planes first can help performance
        m_Planes[0/*Left*/]   = viewT[3] + viewT[0];
        m_Planes[1/*Right*/]  = viewT[3] - viewT[0];
        m_Planes[2/*Bottom*/] = viewT[3] + viewT[1];
        m_Planes[3/*Top*/]    = viewT[3] - viewT[1];
        m_Planes[4/*Near*/]   = viewT[3] + viewT[2];
        m_Planes[5/*Far*/]    = viewT[3] - viewT[2];

        // Normalize the plane.
        for (auto& plane : m_Planes)
        {
            float l = glm::length(glm::vec3(plane));
            plane.x = plane.x / l;
            plane.y = plane.y / l;
            plane.z = plane.z / l;
            plane.w = plane.w / l;
        }
    };

    inline const auto& GetPlanes() const { return m_Planes; }

    /// Test point against the view frustum
    /// @returns if the point is inside or outside the frustum
    /// @note can get some false positives (boxes counted as inside when they are actually outside), additional bounding plane checks would hit performance for these edge cases so currently not implemented.
    inline OctreeBase::eQueryResult PointTest(const glm::vec3& point) const
    {
        const auto point4 = glm::vec4(point, 1.0f);

        // check point outside/inside each plane of frustum
        for (int i = 0; i < 6; i++)
        {
            if (glm::dot(m_Planes[i], point4) < 0.0)
                // outside this plane, so outside the frustum
                return OctreeBase::eQueryResult::Outside;
        }
        return OctreeBase::eQueryResult::Inside;
    }

    /// Test sphere against the view frustum
    /// @returns if the point is inside or outside the frustum
    /// @note can get some false positives (spheres counted as inside when they are actually outside), additional bounding plane checks would hit performance for these edge cases so currently not implemented.
    inline OctreeBase::eQueryResult SphereTest(const glm::vec3& center, float radius) const
    {
        const auto center4 = glm::vec4(center, 1.0f);
        bool partial = false;

        // check sphere outside/inside/fully-inside each plane of frustum
        for (int i = 0; i < 6; i++)
        {
            float dp = glm::dot(m_Planes[i], center4);
            if (dp < -radius)
                // fully outside this plane so fully outside the frustum
                return OctreeBase::eQueryResult::Outside;
            else if (dp <= radius)
                partial = true;
        }
        return partial ? OctreeBase::eQueryResult::Partial  : OctreeBase::eQueryResult::Inside;
    }

    /// Test a box against the view frustum
    /// @returns if the box is inside, outside or partially in the frustum
    /// @note can get some false positives (boxes counted as inside or partial when they are actually outside), additional bounding plane checks would hit performance for these edge cases so currently not implemented.
    inline OctreeBase::eQueryResult BoxTest( const glm::vec3& boxCenter, const glm::vec3& boxHalfSize ) const
    {
        const glm::vec3 boxMin = boxCenter - boxHalfSize;
        const glm::vec3 boxMax = boxCenter + boxHalfSize;

        // check box outside/inside of frustum
        bool partiallyOutside = false;
        for( int i=0; i<6; i++ )
        {
            uint32_t outCount = (glm::dot( m_Planes[i], glm::vec4( boxMin.x, boxMin.y, boxMin.z, 1.0f ) ) < 0.0) ? 1 : 0;
            outCount += (glm::dot( m_Planes[i], glm::vec4(boxMax.x, boxMin.y, boxMin.z, 1.0f) ) < 0.0 ) ? 1 : 0;
            outCount += (glm::dot( m_Planes[i], glm::vec4(boxMin.x, boxMax.y, boxMin.z, 1.0f) ) < 0.0 ) ? 1 : 0;
            outCount += (glm::dot( m_Planes[i], glm::vec4(boxMax.x, boxMax.y, boxMin.z, 1.0f) ) < 0.0 ) ? 1 : 0;
            outCount += (glm::dot( m_Planes[i], glm::vec4(boxMin.x, boxMin.y, boxMax.z, 1.0f) ) < 0.0 ) ? 1 : 0;
            outCount += (glm::dot( m_Planes[i], glm::vec4(boxMax.x, boxMin.y, boxMax.z, 1.0f) ) < 0.0 ) ? 1 : 0;
            outCount += (glm::dot( m_Planes[i], glm::vec4(boxMin.x, boxMax.y, boxMax.z, 1.0f) ) < 0.0 ) ? 1 : 0;
            outCount += (glm::dot( m_Planes[i], glm::vec4(boxMax.x, boxMax.y, boxMax.z, 1.0f) ) < 0.0 ) ? 1 : 0;
            if( outCount == 8 )
                // If all 8 points of box are outside plane we are outside this plane, and by extension outside the view frustum.
                return OctreeBase::eQueryResult::Outside;
            if( outCount != 0 )
                partiallyOutside = true;
        }

        // check frustum outside/inside box

        /* for now dont do this calculation - if we are getting a lot of false positives then implement this.
        False positives will manifest as large objects behind the camera that should have been culled but weren't !
        The downside is the extra CPU time spent double checking all the positive positives! */
        return partiallyOutside ? OctreeBase::eQueryResult::Partial : OctreeBase::eQueryResult::Inside;
    }
private:
    /// Planes defining the frustum sides (for culling)
    glm::vec4 m_Planes[6];
    //// Frustum vertex positions, converted to x,y,z array (for potential vectorization)
    //float m_VerticesX[8];
    //float m_VerticesZ[8];
    //float m_VerticesY[8];
};


/// Helper frustum test functor
/// Use with Octree to query the octree against ViewFrustum.
/// @ingroup Mesh
struct FrustumTest
{
    FrustumTest( const ViewFrustum& frustum ) : m_Frustum(frustum) {
    }

    OctreeBase::eQueryResult operator()( const glm::vec3& center, const glm::vec3& halfSize ) const
    {
        // Expand out the query because we allow objects up to half the cell size in each cell.
        //const glm::vec4 querySize = halfSize + 0.5f * halfSize;
        return m_Frustum.BoxTest( center, halfSize );
    }

    OctreeBase::eQueryResult operator()( const glm::vec3& point ) const
    {
        return m_Frustum.PointTest( point );
    }

    const ViewFrustum& m_Frustum;
};
