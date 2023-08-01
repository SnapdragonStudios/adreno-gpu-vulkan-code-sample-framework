//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <span>
#include "system/glm_common.hpp"
#include "mesh/meshIntermediate.hpp"


/// Container for a single mesh and the positions of all its instances.
/// @ingroup Mesh
struct MeshInstance
{
    MeshObjectIntermediate                           mesh;
    std::vector<MeshObjectIntermediate::FatInstance> instances;
};


/// Mesh Instance Generator helper class
/// Given an array of mesh objects code determines which are identical (outside of position/world-normal)
/// and generates a list of the unique models and their instance transformations.
/// Only works for meshes that are instanced by moving/rotating (no scale changes currently supported).
/// 
/// Internally uses Jacobi SVD (Singular Value Decomposition) to determine the rotation between two candidate mesh objects (candidates are determined by looking for identical UV data).
/// The rotation/translation calculated using SVD is then applied to the vertices in the first candidate to see if that generates the vertex positions of the second candidate - if
/// they do an instance is added and the second mesh discarded.
/// 
/// Can take a non trivial amount of time to calculate depending on numbers of meshes, number of candidate pairs to test and overall vertex count.
/// Bistro exterior (~2m verts) takes ~3 seconds to find all instances (1500 candidate meshes) single threaded on an i9 10900k in debug build.
/// 
/// @ingroup Mesh
class MeshInstanceGenerator
{
public:
    /// Find duplicated mesh instances inside the objects array and group together.  Will detect instances that differ by rotation and translation.
    static std::vector<MeshInstance> FindInstances(std::vector<MeshObjectIntermediate> objects);
    /// Test helper that just moves the objects into an array of MeshInstances (with each output mesh having just one instance).
    static std::vector<MeshInstance> NullFindInstances(std::vector<MeshObjectIntermediate> objects);
};
