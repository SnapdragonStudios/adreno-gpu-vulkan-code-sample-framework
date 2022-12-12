//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "tinyobjloader/tiny_obj_loader.h"
#include "MeshObject.h"
#include "material/vertexFormat.hpp"
#include "memory/vertexBufferObject.hpp"
#include "mesh/meshObjectIntermediate.hpp"
#include "system/assetManager.hpp"

///////////////////////////////////////////////////////////////////////////////

// Format of vertex_layout
VertexFormat MeshObject::vertex_layout::sFormat {
    sizeof(MeshObject::vertex_layout),
    VertexFormat::eInputRate::Vertex,
    {
        { offsetof(MeshObject::vertex_layout, pos),     VertexElementType::Vec3 },  //float pos[3];         // SHADER_ATTRIB_LOC_POSITION
        { offsetof(MeshObject::vertex_layout, normal),  VertexElementType::Vec3 },  //float normal[3];      // SHADER_ATTRIB_LOC_NORMAL
        { offsetof(MeshObject::vertex_layout, uv),      VertexElementType::Vec2 },  //float uv[2];          // SHADER_ATTRIB_LOC_TEXCOORD0
        { offsetof(MeshObject::vertex_layout, color),   VertexElementType::Vec4 },  //float color[4];       // SHADER_ATTRIB_LOC_COLOR
        { offsetof(MeshObject::vertex_layout, tangent), VertexElementType::Vec3 },  //float tangent[3];     // SHADER_ATTRIB_LOC_TANGENT
        //{ offsetof(MeshObject::vertex_layout, bitangent), VertexElementType::Vec3 }, // float binormal[3];  // SHADER_ATTRIB_LOC_BITANGENT
     },
     {"Position", "Normal", "UV", "Color", "Tangent" /*,"Bitangent"*/ }
};

///////////////////////////////////////////////////////////////////////////////

MeshObject::MeshObject() : m_NumVertices(0)
{
}

///////////////////////////////////////////////////////////////////////////////

MeshObject& MeshObject::operator=(MeshObject&& other) noexcept
{
    if (this != &other)
    {
        m_VertexBuffers = std::move(other.m_VertexBuffers);
        m_IndexBuffer = std::move(other.m_IndexBuffer);
        m_NumVertices = other.m_NumVertices;
        other.m_NumVertices = 0;
    }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

MeshObject::MeshObject(MeshObject&& other) noexcept
{
    *this = std::move(other);
}

///////////////////////////////////////////////////////////////////////////////

MeshObject::~MeshObject()
{
    Destroy();
}

///////////////////////////////////////////////////////////////////////////////

bool MeshObject::Destroy()
{
    m_NumVertices = 0;
    m_VertexBuffers.clear();
    m_IndexBuffer.reset();
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Loads a .gltf file and builds a single Vertex Buffer containing relevant vertex.
// DOES not use (or populate) the index buffer although it does respect the gltf index buffer when loading the mesh
// positions, normals and threadColors. Vertices are held in TRIANGLE_LIST format.
//
// Vertex layout corresponds to MeshObject::vertex_layout structure.
bool MeshObject::LoadGLTF(Vulkan* pVulkan, AssetManager& assetManager, const std::string& filename, uint32_t binding, MeshObject* meshObject)
{
    const auto meshObjects = MeshObjectIntermediate::LoadGLTF(assetManager, filename);

    if (meshObjects.empty())
        return false;

    return MeshObject::CreateMesh(pVulkan, meshObjects[0].CopyFlattened()/*remove indices*/, binding, { &vertex_layout::sFormat, 1 }, meshObject);
}

///////////////////////////////////////////////////////////////////////////////

// Builds a screen space mesh as a single Vertex Buffer containing relevant vertex
// positions, normals and threadColors. Vertices are held in TRIANGLE_LIST format.
//
// Vertex layout corresponds to MeshObject::vertex_layout structure.
bool MeshObject::CreateScreenSpaceMesh(Vulkan* pVulkan, glm::vec4 PosLLRadius, glm::vec4 UVLLRadius, uint32_t binding, MeshObject* meshObject)
{
    // Each quad is just a list of 6 verts (two triangles) with 4 floats per vert ...
    // ... and a 4 float color for each vert
    float Verts[6][4];
    // float Color[6][4];   <= If we want different color per vert

    // This is the same for everyone
    float VertColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Note have to invert the Y-Values for Vulkan

    // [0,1,2]
    Verts[0][0] = PosLLRadius[0];
    Verts[0][1] = PosLLRadius[1];
    Verts[0][2] = UVLLRadius[0];
    Verts[0][3] = 1.0f - UVLLRadius[1];

    Verts[1][0] = PosLLRadius[0] + PosLLRadius[2];
    Verts[1][1] = PosLLRadius[1];
    Verts[1][2] = UVLLRadius[0] + UVLLRadius[2];
    Verts[1][3] = 1.0f - UVLLRadius[1];

    Verts[2][0] = PosLLRadius[0];
    Verts[2][1] = PosLLRadius[1] + PosLLRadius[3];
    Verts[2][2] = UVLLRadius[0];
    Verts[2][3] = 1.0f - (UVLLRadius[1] + UVLLRadius[3]);


    // [1, 3, 2]
    Verts[3][0] = PosLLRadius[0] + PosLLRadius[2];
    Verts[3][1] = PosLLRadius[1];
    Verts[3][2] = UVLLRadius[0] + UVLLRadius[2];
    Verts[3][3] = 1.0f - UVLLRadius[1];

    Verts[4][0] = PosLLRadius[0] + PosLLRadius[2];
    Verts[4][1] = PosLLRadius[1] + PosLLRadius[3];
    Verts[4][2] = UVLLRadius[0] + UVLLRadius[2];
    Verts[4][3] = 1.0f - (UVLLRadius[1] + UVLLRadius[3]);

    Verts[5][0] = PosLLRadius[0];
    Verts[5][1] = PosLLRadius[1] + PosLLRadius[3];
    Verts[5][2] = UVLLRadius[0];
    Verts[5][3] = 1.0f - (UVLLRadius[1] + UVLLRadius[3]);

    // Each quad is just a list of 6 verts (two triangles) with 4 floats per vert
    std::vector<vertex_layout> objVertices;
    for (uint32_t WhichVert = 0; WhichVert < 6; WhichVert++)
    {
        vertex_layout vertex {};

        // Position data is the first part of the quad
        vertex.pos[0] = Verts[WhichVert][0];
        vertex.pos[1] = Verts[WhichVert][1];
        vertex.pos[2] = 0.0f;

        // UV data is the second part of the quad
        vertex.uv[0] = Verts[WhichVert][2];
        vertex.uv[1] = Verts[WhichVert][3];

        vertex.normal[0] = 0.0f;
        vertex.normal[1] = 0.0f;
        vertex.normal[2] = 1.0f;

        // Don't really have tangents
        vertex.tangent[0] = 0.0f;
        vertex.tangent[1] = 0.0f;
        vertex.tangent[2] = 0.0f;

        // Default vertice color is white (debug with pink if needed)
        vertex.color[0] = VertColor[0];
        vertex.color[1] = VertColor[1];
        vertex.color[2] = VertColor[2];
        vertex.color[3] = VertColor[3];

        objVertices.push_back(vertex);
    }

    meshObject->m_NumVertices = (uint32_t)objVertices.size();
    meshObject->m_VertexBuffers.resize(1);
    meshObject->m_VertexBuffers[0].Initialize(&pVulkan->GetMemoryManager(),
        objVertices.size(),
        &objVertices[0]);

    meshObject->m_VertexBuffers[0].AddBindingAndAtributes(binding, vertex_layout::sFormat);

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool MeshObject::CreateMesh(Vulkan* pVulkan, const MeshObjectIntermediate& meshObject, uint32_t bindingIndex, const tcb::span<const VertexFormat> pVertexFormat, MeshObject* meshObjectOut)
{
    assert(pVulkan);
    assert(meshObjectOut);
    meshObjectOut->Destroy();

    MemoryManager& memoryManager = pVulkan->GetMemoryManager();

    const size_t numVertices = meshObject.m_VertexBuffer.size();
    // It is valid to have no vertex buffers (empty pVertexFormat) but still render vertices... vertex shader could generate verts procedurally.
    meshObjectOut->m_NumVertices = (uint32_t)numVertices;

    //
    // We can have more than one buffer in a 'mesh'.  Use this to do things like splitting the position away from uv/color/normal etc so the shadow passes use less vertex bandwidth.
    // We also want to skip any Instance bindings (not handled by the Mesh).
    // Beware of putting instance (rate) bindings in the 'middle' of vertex rate buffers as the output MeshObject::m_VertexBuffers cannot be all bound in one chunk in that case.
    //
    for (uint32_t vertexBufferIdx = 0; vertexBufferIdx < pVertexFormat.size(); ++vertexBufferIdx)
    {
        // Convert from the 'fat' vertex data to the required destination format for this vertex buffer.
        const auto& vertexFormat = pVertexFormat[vertexBufferIdx];
        if (vertexFormat.inputRate == VertexFormat::eInputRate::Vertex)
        {
            const std::vector<uint32_t> formattedVertexData = MeshObjectIntermediate::CopyFatVertexToFormattedBuffer(meshObject.m_VertexBuffer, pVertexFormat[vertexBufferIdx]);

            if (!meshObjectOut->m_VertexBuffers.emplace_back().Initialize(&memoryManager, vertexFormat.span, numVertices, formattedVertexData.data()))
            {
                LOGE("Cannot Initialize vertex buffer %d", vertexBufferIdx);
                return false;
            }
            meshObjectOut->m_VertexBuffers.rbegin()->AddBindingAndAtributes(bindingIndex + vertexBufferIdx, vertexFormat);
        }
    }

    // Create the index buffer (with an appropriate VkIndexType index type, IF the meshObject has index data).
    if (!CreateIndexBuffer(memoryManager, meshObject, meshObjectOut->m_IndexBuffer))
    {
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool MeshObject::CreateIndexBuffer(MemoryManager& memoryManager, const MeshObjectIntermediate& meshObject, std::optional<IndexBufferObject>& indexBufferOut, VkBufferUsageFlags usage)
{
    //
    // If the source has an index buffer then copy the data in to a Vulkan buffer
    //
    if (!std::visit(
        [&meshObject, &indexBufferOut, &memoryManager, usage](auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::vector<uint32_t>>) {
                auto& indexBuffer = indexBufferOut.emplace(VK_INDEX_TYPE_UINT32);
                const auto& vec32 = std::get<std::vector<uint32_t>>(meshObject.m_IndexBuffer);
                if (!indexBuffer.Initialize(&memoryManager, vec32.size(), vec32.data(), false, usage))
                {
                    return false;
                }
            }
            else if constexpr (std::is_same_v<T, std::vector<uint16_t>>) {
                auto& indexBuffer = indexBufferOut.emplace(VK_INDEX_TYPE_UINT16);
                const auto& vec16 = std::get<std::vector<uint16_t>>(meshObject.m_IndexBuffer);
                if (!indexBuffer.Initialize(&memoryManager, vec16.size(), vec16.data(), false, usage))
                {
                    return false;
                }
            }
            return true;
        },
        meshObject.m_IndexBuffer))
    {
        LOGE("Cannot Initialize index buffer");
        indexBufferOut.reset();
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////

// Lengyel, Eric. "Computing Tangent Space Basis Vectors for an Arbitrary Mesh". Terathon Software 3D Graphics Library, 2001.
static void CalculateFaceTangentAndBitangent(const float* objPos0, const float* objPos1, const float* objPos2,
                                             const float* ObjUv0, const float* ObjUv1, const float* ObjUv2,
                                             glm::vec3& face_tangent, glm::vec3& face_bitangent)
{
    glm::vec3 v0(objPos0[0], objPos0[1], objPos0[2]);
    glm::vec3 v1(objPos1[0], objPos1[1], objPos1[2]);
    glm::vec3 v2(objPos2[0], objPos2[1], objPos2[2]);
    glm::vec2 uv0(ObjUv0[0], ObjUv0[1]);
    glm::vec2 uv1(ObjUv1[0], ObjUv1[1]);
    glm::vec2 uv2(ObjUv2[0], ObjUv2[1]);
    glm::vec3 vdist1 = v1 - v0;
    glm::vec3 vdist2 = v2 - v0;
    glm::vec2 uvdist1 = uv1 - uv0;
    glm::vec2 uvdist2 = uv2 - uv0;

    const float r = 1.0f / (uvdist1.x * uvdist2.y - uvdist2.x * uvdist1.y);
    face_tangent = { (uvdist2.y * vdist1.x - uvdist1.y * vdist2.x) * r, (uvdist2.y * vdist1.y - uvdist1.y * vdist2.y) * r, (uvdist2.y * vdist1.z - uvdist1.y * vdist2.z) * r };
    face_bitangent = { (uvdist1.x * vdist2.x - uvdist2.x * vdist1.x) * r, (uvdist1.x * vdist2.y - uvdist2.x * vdist1.y) * r, (uvdist1.x * vdist2.z - uvdist2.x * vdist1.z) * r };
}
