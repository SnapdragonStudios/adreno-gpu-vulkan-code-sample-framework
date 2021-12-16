// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <optional>
#include <string>
#include <variant>
#include "vulkan/vulkan.hpp"
#include "system/glm_common.hpp"
#include "memory/indexBufferObject.hpp"

// Forward declarations
class VertexFormat;
class MeshObject;
class AssetManager;
class VertexBufferObject;

// Under Vulkan we use uniform buffer objects in the shader. One for vert and one for frag
#define SHADER_VERT_UBO_LOCATION            0
#define SHADER_FRAG_UBO_LOCATION            1

// Texture Locations
#define SHADER_BASE_TEXTURE_LOC             2

/// Defines a simple object for creating and holding intermediae vertex data in a 'Fat' format (all available data).
class MeshObjectIntermediate
{
    MeshObjectIntermediate(const MeshObject&) = delete;
    MeshObjectIntermediate& operator=(const MeshObject&) = delete;
public:
    MeshObjectIntermediate() {}
    MeshObjectIntermediate(MeshObjectIntermediate&&) noexcept = default;
    MeshObjectIntermediate& operator=(MeshObjectIntermediate&&) noexcept = default;

    /// Clear/delete the vectors associated with the MeshObjectIntermediate
    void Release();

    /// Loads a .obj and .mtl file and builds a single vector array containing an object
    /// for each shape that contains all vertex positions, normals, materials and colors.
    static std::vector<MeshObjectIntermediate> LoadObj(AssetManager& assetManager, const std::string& filename);

    /// Loads a .gltf file and builds a single vector array containing an object
    /// for each shape in the gltf that contains all vertex positions, normals and colors along with an array of the materials in the gltf file.
    static std::vector<MeshObjectIntermediate> LoadGLTF(AssetManager& assetManager, const std::string& filename);

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

    /// High level description of a material attached to the vertexbuffer (and indexed by FatVertex::material)
    struct MaterialDef
    {
        std::string     diffuseFilename;
        std::string     bumpFilename;
        std::string     emissiveFilename;
        std::string     specMapFilename;
        bool            alphaCutout = false;
        bool            transparent = false;
    };

public:
    std::vector<FatVertex>      m_VertexBuffer;
    /// Index buffer can be 16bit or 32bit (or not exist; in which case every 3 vertices in m_VertexBuffer are the verts of a triangle).
    std::variant<std::monostate, std::vector<uint32_t>, std::vector<uint16_t>> m_IndexBuffer;
    std::vector<MaterialDef>    m_Materials;
};


/// Defines a simple object for creating and holding Vulkan state corresponding to a single mesh.
class MeshObject
{
    MeshObject(const MeshObject&) = delete;
    MeshObject& operator=(const MeshObject&) = delete;
public:
    MeshObject();
    MeshObject(MeshObject&&) noexcept;
    MeshObject& operator=(MeshObject&&) noexcept;
    ~MeshObject();
    
    /// Create a MeshObject for a single .obj file (no materials)
    /// @returns true on success
    static bool LoadObj(Vulkan* pVulkan, AssetManager&, const std::string& objFilename, uint32_t binding, MeshObject* meshObject);
    /// Create a MeshObject for the first shape in a .gltf file (no materials)
    /// @returns true on success
    static bool LoadGLTF(Vulkan* pVulkan, AssetManager&, const std::string& filename, uint32_t binding, MeshObject* meshObject);

    /// Builds a screen space mesh as a single Vertex Buffer containing relevant vertex positions, normals and colors. Vertices are held in TRIANGLE_LIST format.
    /// Vertex layout corresponds to MeshObject::vertex_layout structure.
    /// @returns true on success
    static bool CreateScreenSpaceMesh(Vulkan* pVulkan, glm::vec4 PosLLRadius, glm::vec4 UVLLRadius, uint32_t binding, MeshObject* meshObject);
    /// Helper for CreateScreenSpaceMesh that has a x,y position from -1,-1 to 1,1 and UV from 0,0 to 1,1.
    /// @returns true on success
    static bool CreateScreenSpaceMesh(Vulkan* pVulkan, uint32_t binding, MeshObject* meshObject)
    {
        return CreateScreenSpaceMesh(pVulkan, glm::vec4(-1.0f, -1.0f, 2.0f, 2.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), binding, meshObject);
    }

    /// Create a MeshObject from a 'fat' MeshObjectIntermediate object, rearranging the vertex data to match the supplied vertex format(s).
    /// Can have multiple VertexFormats, which will create multiple vertex buffers (eg if we want to split vertex position data away from other vertex attributes)
    /// @param pVertexFormat format of the vertex data being output
    /// @returns true on success
    static bool CreateMesh(Vulkan* pVulkan, const MeshObjectIntermediate& meshObject, uint32_t binding, const tcb::span<const VertexFormat> pVertexFormat, MeshObject* meshObjectOut);

    /// Creates a 'raw' array of data from a 'fat' MeshObjectIntermediate object, with the returned data being formatted in the way described by vertexFormat
    /// @returns data in the requested vertexFormat
    static std::vector<uint32_t> CopyFatToFormattedBuffer(const tcb::span<const MeshObjectIntermediate::FatVertex>& fatVertexBuffer, const VertexFormat& vertexFormat);

    virtual bool Destroy();

    /// Helper to create a IndexBufferObject, IF the mesh object has index buffer data.
    /// @returns true for success (including no index buffer data existing in IndexBufferObject), false on error.
    static bool CreateIndexBuffer(MemoryManager& memoryManager, const MeshObjectIntermediate& meshObject, std::optional<IndexBufferObject>& indexBufferOut, VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // These MUST match the order of the attrib locations and sFormat must reflect the layout of this struct too!
    struct vertex_layout
    {
        float pos[3];           // SHADER_ATTRIB_LOC_POSITION
        float normal[3];        // SHADER_ATTRIB_LOC_NORMAL
        float uv[2];            // SHADER_ATTRIB_LOC_TEXCOORD0
        float color[4];         // SHADER_ATTRIB_LOC_COLOR
        float tangent[3];       // SHADER_ATTRIB_LOC_TANGENT
        // float binormal[3];      // SHADER_ATTRIB_LOC_BITANGENT
        static VertexFormat sFormat;
    };
    
public:
    uint32_t                        m_NumVertices;
    std::vector<VertexBufferObject> m_VertexBuffers;
    std::optional<IndexBufferObject>m_IndexBuffer;
};
