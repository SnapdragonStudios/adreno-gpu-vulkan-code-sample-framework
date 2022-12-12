//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <string>
#include <variant>
#include <vector>
#include "system/glm_common.hpp"
#include "tcb/span.hpp"
#include "json/include/nlohmann/json_fwd.hpp"

// Forward declarations
class AssetManager;
class VertexFormat;
namespace tinygltf {
    class Model;
};


/// Defines a simple object for creating and holding intermediate vertex data in a 'Fat' format (all available data).
class MeshObjectIntermediate
{
    MeshObjectIntermediate(const MeshObjectIntermediate&) = delete;
    MeshObjectIntermediate& operator=(const MeshObjectIntermediate&) = delete;
public:
    MeshObjectIntermediate() {}
    MeshObjectIntermediate(MeshObjectIntermediate&&) noexcept = default;
    MeshObjectIntermediate& operator=(MeshObjectIntermediate&&) noexcept = default;

    /// Clear/delete the vectors associated with the MeshObjectIntermediate
    void Release();

    /// Bake the object transform in to the vertex data (positions/normals/trangents/bitangents) and clear the transform (to identity)
    void BakeTransform();

    /// Create a new mesh with index data 'flattened' into the vertex data (so 3 vertices per triangle and duplication where appropriate)
    MeshObjectIntermediate CopyFlattened() const;

    /// Loads a .obj and .mtl file and builds a single vector array containing an object
    /// for each shape that contains all vertex positions, normals, materials and colors.
    static std::vector<MeshObjectIntermediate> LoadObj(AssetManager& assetManager, const std::string& filename);

    /// Loads a .gltf file and builds a single vector array containing an object
    /// for each shape in the gltf that contains all vertex positions, normals and colors along with an array of the materials in the gltf file.
    /// @param ignoreTransforms Dont use the gltf node transforms (generate MeshObjectIntermediate with the indentity position/rotation/scale).  Still applies globalScale!
    /// @param globalScale Additional scale applied to all objects loaded from the gltf
    static std::vector<MeshObjectIntermediate> LoadGLTF(AssetManager& assetManager, const std::string& filename, bool ignoreTransforms = true, const glm::vec3 globalScale = glm::vec3(1.0f, 1.0f, 1.0f));

    /// Builds a screen space intermediate mesh (6 verts) containing relevant vertex positions, normals and colors.
    static MeshObjectIntermediate CreateScreenSpaceMesh(glm::vec4 PosLLRadius, glm::vec4 UVLLRadius);
    /// Helper for CreateScreenSpaceMesh that has a x,y position from -1,-1 to 1,1 and UV from 0,0 to 1,1.
    static MeshObjectIntermediate CreateScreenSpaceMesh()
    {
        return CreateScreenSpaceMesh(glm::vec4(-1.0f, -1.0f, 2.0f, 2.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    }
    /// Builds a 'mesh' with a single triangle that covers the entire screen
    static MeshObjectIntermediate CreateScreenSpaceTriMesh();

    /// This is ALL the data that could be loaded from a mesh.
    /// Expected to be copied into one (or more) shader specific layouts
    struct FatVertex
    {
        float position[3];
        float normal[3];
        float color[4];
        float uv0[2];
        float tangent[3];
        float bitangent[3];
        int   material;     ///< indexc in to m_Materials
    };

    /// All the data that could be associuated with an object instance
    struct FatInstance
    {
        /// Instance transform.
        /// @note This is transposed (row major) (GLSL is generally column major).
        typedef glm::mat3x4 tInstanceTransform;

        tInstanceTransform  transform;
        int                 nodeId;
    };

    /// Creates a 'raw' array of data from a 'fat' MeshObjectIntermediate object, with the returned data being formatted in the way described by vertexFormat
    /// @returns data in the requested vertexFormat
    static std::vector<uint32_t> CopyFatVertexToFormattedBuffer(const tcb::span<const MeshObjectIntermediate::FatVertex>& fatVertexBuffer, const VertexFormat& vertexFormat);

    /// Creates a 'raw' array of data from a 'fat instance' MeshObjectIntermediate object, with the returned data being formatted in the way described by vertexFormat
    /// Same functionality as @CopyFatVertexToFormattedBuffer but for instance rate data.
    /// @returns data in the requested vertexFormat
    static std::vector<uint32_t> CopyFatInstanceToFormattedBuffer(const tcb::span<const MeshObjectIntermediate::FatInstance>& fatVertexBuffer, const VertexFormat& vertexFormat);

    /// High level description of a material attached to the vertexbuffer (and indexed by FatVertex::material)
    struct MaterialDef
    {
        std::string           diffuseFilename;
        std::string           bumpFilename;
        std::string           emissiveFilename;
        std::string           specMapFilename;
        bool                  alphaCutout     = false;
        bool                  transparent     = false;
        std::array<double, 4> baseColorFactor = { 1.0, 1.0, 1.0, 1.0 };
        double                metallicFactor  = 1.0;
        double                roughnessFactor = 1.0;

        friend void to_json( nlohmann::json& j, const MeshObjectIntermediate::MaterialDef& material );
        friend void from_json( const nlohmann::json& j, MeshObjectIntermediate::MaterialDef& material );
    };

public:
    typedef std::vector<FatVertex> tVertexBuffer;
    typedef std::variant<std::monostate, std::vector<uint32_t>, std::vector<uint16_t>> tIndexBuffer;

    tVertexBuffer               m_VertexBuffer;
    /// Index buffer can be 16bit or 32bit (or not exist; in which case every 3 vertices in m_VertexBuffer are the verts of a triangle).
    tIndexBuffer                m_IndexBuffer;
    std::vector<MaterialDef>    m_Materials;

    /// World position transform for this mesh object
    glm::mat4                   m_Transform = glm::identity<glm::mat4>();
    /// Node id (child node that this node is attached to) from gltf, can be used to lookup animations on this node (non skinned animation)
    int                         m_NodeId = -1;
};


/// @brief Helper class/functor to parse a tinygltf::Model into a vector of MeshObjectIntermediate
struct MeshObjectIntermediateGltfProcessor
{
    MeshObjectIntermediateGltfProcessor(const MeshObjectIntermediateGltfProcessor&) = delete;
    MeshObjectIntermediateGltfProcessor& operator=(const MeshObjectIntermediateGltfProcessor&) = delete;

    MeshObjectIntermediateGltfProcessor(const std::string& filename, bool ignoreTransforms, const glm::vec3 globalScale) : m_filename(filename), m_ignoreTransforms(ignoreTransforms), m_globalScale(globalScale) {}
    bool operator()(const tinygltf::Model& ModelData);

    const std::string m_filename;
    const bool m_ignoreTransforms;
    const glm::vec3 m_globalScale;
    std::vector<MeshObjectIntermediate> m_meshObjects;
};
