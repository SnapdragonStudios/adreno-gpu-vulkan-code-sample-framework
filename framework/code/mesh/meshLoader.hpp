//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "system/glm_common.hpp"
#include <vector>
#include <span>
#include <string>

#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRIE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define JSON_NOEXCEPTION
#include "tinygltf/tiny_gltf.h"

class AssetManager;


/// Mesh Loader class
/// Provides a top-level (templated) loader class for gltf model objects.
/// Does not specify how the gltf data is interpreted/used (that is down to the templated classes) but handles loading/parsing the raw gltf file and provides this to the interested classes.
/// 
/// @ingroup Mesh
class MeshLoader
{
private:
    // Templated helper to run the 'currentProcessor' lambda function (recursively) each of the lambdas in subsequentProcessors
    template<typename T, typename... TT>
    static bool ExecuteModelDataProcessor(const tinygltf::Model& modelData, T && currentProcessor, TT && ... subsequentProcessors)
    {
        if (!currentProcessor(modelData))
            return false;                                                         // error
        else if constexpr (sizeof...(subsequentProcessors) == 0)
            return true;                                                          // no more processors to execute.  success!
        else
            return ExecuteModelDataProcessor(modelData, subsequentProcessors...); // Recursively call the subsequent processor(s).
    }

public:

    /// @brief Load the named gltf file (using the provided assetManager) and run each of the provided 'modelProcessors' lambda functions.
    /// @tparam ...T lambda function type (expected to take one parameter 'const tinygltf::Model&' and return a bool true on success)
    /// @param assetManager 
    /// @param filename 
    /// @param ...modelProcessors variadic pack of lambda functions (or functors) to execute in order.
    /// @return true if sucessful
    template<typename... T>
    static bool LoadGltf(AssetManager& assetManager, const std::string& filename, T && ... modelProcessors)
    {
        tinygltf::Model ModelData;
        if (!LoadGlftModel(assetManager, filename, ModelData))
        {
            return false;
        }
        return ExecuteModelDataProcessor(ModelData, modelProcessors...);
    }

    /// Internal helper class for converting tinygltf::Node transform to a matrix and concatenating transforms in the hierarchy.
    struct NodeTransform {
        NodeTransform() : Matrix(glm::identity<glm::mat4>()) {}
        NodeTransform(const tinygltf::Node& Node);
        NodeTransform(const NodeTransform& Parent, const tinygltf::Node& Node);
        operator glm::mat4() const { return Matrix; }
        const glm::mat4 Matrix;
    };

    /// Helper for processing (reading) node data in a gltf model.  Recurses through the model hierarchy and calls the provided 'processLambda' templated function for each node (in hierarchical order).
    template<typename T_PROCESSFN>
    static bool RecurseModelNodes(const tinygltf::Model& ModelData, const std::vector<int>& SceneNodeIndices, const T_PROCESSFN& processLambda)
    {
        // Define a lambda inside RecurseModelNodes template so we can call recursively!
        const auto& recurse_lambda = [&processLambda](const tinygltf::Model& ModelData, const NodeTransform &ParentTransform, const std::vector<int>& SceneNodeIndices, const auto& lambda_ref) -> bool {
            for (int sceneNodeIdx : SceneNodeIndices) {

                const tinygltf::Node& NodeData = ModelData.nodes[sceneNodeIdx];
                NodeTransform Transform(ParentTransform, NodeData);

                if (!processLambda(ModelData, Transform, NodeData))
                    return false;   // return 'error'

                if (!NodeData.children.empty())
                {
                    if (!lambda_ref(ModelData, Transform, NodeData.children, lambda_ref))
                        return false;   // return 'error'
                }
            }
            return true;    // return 'success'
        };
        NodeTransform Transform;
        return recurse_lambda(ModelData, Transform, SceneNodeIndices, recurse_lambda);
    }

protected:
    /// Internal helper to load the named gltf file 'filename' (using the assetManager)
    /// @param ModelData output ModelData
    /// @return true on success, false on error.
    static bool LoadGlftModel(AssetManager& assetManager, const std::string& filename, tinygltf::Model& ModelData);
};


/// Helper model processor to check the "Scene" is as we are expecting before doing any loading from the gltf Model.
/// Pass in as the (1st) processor parameter to MeshLoader::LoadGltf()
/// eg: MeshLoader::LoadGltf( assetmanager, filename, MeshLoaderModelSceneSanityCheck(filename), ...more processors...)
struct MeshLoaderModelSceneSanityCheck
{
    MeshLoaderModelSceneSanityCheck& operator=(const MeshLoaderModelSceneSanityCheck&) = delete;
    MeshLoaderModelSceneSanityCheck(const MeshLoaderModelSceneSanityCheck&) = delete;
    MeshLoaderModelSceneSanityCheck(const std::string& filename) : m_filename(filename) {}
    bool operator()(const tinygltf::Model& ModelData);
    const std::string m_filename;
};
