//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "meshIntermediate.hpp"
#include "tinyobjloader/tiny_obj_loader.h"
#include "material/vertexFormat.hpp"
#include "system/os_common.h"
#include "system/assetManager.hpp"
#include "system/glm_common.hpp"
#include "system/crc32c.hpp"
#include "mesh/meshLoader.hpp"
#include "nlohmann/json.hpp"
#include <istream>
#include <sstream>
#include <set>

using Json = nlohmann::json;

///////////////////////////////////////////////////////////////////////////////

// glTF supports : POSITION, NORMAL, TANGENT, TEXCOORD_0, TEXCOORD_1, COLOR_0, JOINTS_0, and WEIGHTS_0
enum glTF_ATTRIBS
{
    ATTRIB_INDICES = 0,
    ATTRIB_POSITION,
    ATTRIB_NORMAL,
    ATTRIB_TANGENT,
    ATTRIB_TEXCOORD_0,
    ATTRIB_TEXCOORD_1,
    ATTRIB_COLOR_0,
    ATTRIB_JOINTS_0,
    ATTRIB_WEIGHTS_0,
    NUM_GLTF_ATTRIBS
};

typedef struct _gltfAttribInfo
{
    // Index into the accessor list
    int32_t         AccessorIndx = -1;

    // Number of elements
    uint32_t        Count = 0;

    // Number of bytes per element (span)
    uint32_t        BytesPerElem = 0;

    // Total number of bytes
    size_t          BytesTotal = 0;

    // Pointer to data within the glTF buffer
    void*           pData = nullptr;

} gltfAttribInfo;


///////////////////////////////////////////////////////////////////////////////

class MaterialFileReader : public tinyobj::MaterialReader {
public:
    explicit MaterialFileReader(AssetManager& assetManager, const std::string& mtl_basedir)
        : m_AssetManager(assetManager), m_mtlBaseDir(mtl_basedir) {}
    virtual ~MaterialFileReader() override {}
    virtual bool operator()(const std::string& matId,
        std::vector<tinyobj::material_t>* materials,
        std::map<std::string, int>* matMap, std::string* warn,
        std::string* err) override;
private:
    AssetManager& m_AssetManager;
    std::string m_mtlBaseDir;
};


bool MaterialFileReader::operator()(const std::string& matId,
    std::vector<tinyobj::material_t>* materials,
    std::map<std::string, int>* matMap,
    std::string* warn, std::string* err)
{
    std::string filepath;
    if (!m_mtlBaseDir.empty())
        filepath = m_AssetManager.JoinPath(m_mtlBaseDir, matId);
    else
        filepath = matId;
    AssetMemStream<char> mtlFile;
    if (m_AssetManager.LoadFileIntoMemory(filepath, mtlFile))
    {
        LoadMtl(matMap, materials, &mtlFile, warn, err);
        return true;
    }

    // error
    std::stringstream ss;
    ss << "Material file [ " << filepath << " ] not found in a path : " << m_mtlBaseDir << std::endl;
    if (warn) {
        (*warn) += ss.str();
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////

//static bool Gltf_FileExists(const std::string& abs_filename, void* vAssetManager)
//{
//    //AssetManager* pAssetManager = (AssetManager*)vAssetManager;
//    return true;
//}
//static std::string Gltf_ExpandFilePath(const std::string& filename, void* vAssetManager)
//{
//    return filename;
//}
//static bool Gltf_ReadWholeFile(std::vector<unsigned char>* output, std::string* error, const std::string& filename, void* vAssetManager)
//{
//    AssetManager* pAssetManager = (AssetManager*)vAssetManager;
//    return pAssetManager->LoadFileIntoMemory(filename, *output);
//}
//static bool Gltf_WriteWholeFile(std::string*, const std::string&, const std::vector<unsigned char>&, void*)
//{
//    return false;
//}

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

///////////////////////////////////////////////////////////////////////////////

static void CalculateTangentAndBitangent(const float* objNormal, const glm::vec3 face_tangent, const glm::vec3 face_bitangent, glm::vec3& tangent/*out*/, glm::vec3& bitangent/*out*/)
{
    glm::vec3 normal(objNormal[0], objNormal[1], objNormal[2]);
    // Gram-Schmidt orthogonalize
    tangent = glm::normalize(face_tangent - normal * glm::dot(normal, face_tangent));
    // Calculate handedness
    float handedness = (glm::dot(glm::cross(normal, tangent), face_bitangent) < 0.0f) ? -1.0f : 1.0f;
    bitangent = handedness * glm::cross(normal, tangent);
}

///////////////////////////////////////////////////////////////////////////////

void MeshObjectIntermediate::Release()
{
    std::vector<FatVertex>().swap(m_VertexBuffer);  // use swap so we know the memory disappears (clear may leave the memory 'reserved').
    std::vector<MaterialDef>().swap(m_Materials);
    m_Transform = glm::identity<glm::mat4>();
    m_NodeId = -1;
}

///////////////////////////////////////////////////////////////////////////////

void MeshObjectIntermediate::BakeTransform()
{
    for (auto& vert : m_VertexBuffer)
    {
        auto transformed = m_Transform * glm::vec4(vert.position[0], vert.position[1], vert.position[2], 1.0f);
        vert.position[0] = transformed.x;
        vert.position[1] = transformed.y;
        vert.position[2] = transformed.z;

        // make the assumption that the m_Transform matrix is a 'standard' TRS matrix and doesnt have skew or anything unusual (which will cause the matrix to not be orthonormal).  If it is possibly not a TRS matrix consider glm::decompose (slow)
        glm::mat3 rotation = glm::mat3(m_Transform);
        rotation[0] = glm::normalize(rotation[0]);
        rotation[1] = glm::normalize(rotation[1]);
        rotation[2] = glm::normalize(rotation[2]);
        ///TODO: test for orthonormality!
        auto rotated = rotation * glm::vec3(vert.normal[0], vert.normal[1], vert.normal[2]);
        vert.normal[0] = rotated.x;
        vert.normal[1] = rotated.y;
        vert.normal[2] = rotated.z;
        rotated = rotation * glm::vec3(vert.tangent[0], vert.tangent[1], vert.tangent[2]);
        vert.tangent[0] = rotated.x;
        vert.tangent[1] = rotated.y;
        vert.tangent[2] = rotated.z;
        rotated = rotation * glm::vec3(vert.bitangent[0], vert.bitangent[1], vert.bitangent[2]);
        vert.bitangent[0] = rotated.x;
        vert.bitangent[1] = rotated.y;
        vert.bitangent[2] = rotated.z;
    }
    // Clear out the tranform now it has been applied
    m_Transform = glm::identity<glm::mat4>();
    // Clear out m_NodeId as it is likely unusable (at least for animations, revisit if m_NodeId is used for other functionality)
    m_NodeId = -1;
}

///////////////////////////////////////////////////////////////////////////////

MeshObjectIntermediate MeshObjectIntermediate::CopyFlattened() const
{
    MeshObjectIntermediate dst;

    std::visit([&](auto& m)
        {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, std::vector<uint32_t>> || std::is_same_v<T, std::vector<uint16_t>>)
            {
                dst.m_VertexBuffer.reserve(m.size());
                for (const auto index : m )
                {
                    dst.m_VertexBuffer.push_back(this->m_VertexBuffer[index]);
                }
            }
            else
            {
                dst.m_VertexBuffer = m_VertexBuffer;
            }
        }, this->m_IndexBuffer);
    dst.m_Materials = m_Materials;
    dst.m_Transform = m_Transform;
    dst.m_NodeId = m_NodeId;
    return dst;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<MeshObjectIntermediate> MeshObjectIntermediate::LoadObj(AssetManager& assetManager, const std::string& filename)
{
    std::vector<MeshObjectIntermediate> meshObjects;

    tinyobj::attrib_t                   attrib;
    std::vector<tinyobj::shape_t>       shapes;
    std::vector<tinyobj::material_t>    materials;

    {
        std::string warn;
        std::string err;

        // Load the obj file in to memory.
        AssetMemStream<char> objFile;
        if ( !assetManager.LoadFileIntoMemory(filename, objFile) )
        {
            LOGE("tinyobj failed to open: %s", filename.c_str());
            return {};
        }

        // Create a stream from the loaded string data.
        MaterialFileReader matFileReader(assetManager, filename);

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &objFile, &matFileReader);

        if (!err.empty())
        {
            LOGE("tinyobj error (%s) loading %s", err.c_str(), filename.c_str());
            return {};
        }

        if (!ret)
        {
            LOGE("tinyobj failed to load %s", filename.c_str());
            return {};
        }
    }

    LOGI("Mesh object %s has %d Shape[s]", filename.c_str(), (int)shapes.size());
    meshObjects.reserve(shapes.size());

    // Mesh may (or may not) have color
    const bool meshHasColor = attrib.vertices.size() == attrib.colors.size();

    // Loop over shapes
    for (size_t WhichShape = 0; WhichShape < shapes.size(); WhichShape++)
    {
        uint32_t objVerticesSize = 0;

        uint32_t NumIndices = (uint32_t)shapes[WhichShape].mesh.indices.size();

        LOGI("    Shape %d:", (int)WhichShape);
        LOGI("      %d Indices (%d triangles)", NumIndices, NumIndices / 3);
        LOGI("      %d Positions", (int)attrib.vertices.size() / 3);
        LOGI("      %d Normals", (int)attrib.normals.size() / 3);
        LOGI("      %d UVs", (int)attrib.texcoords.size() / 2);

        static const uint32_t cMaxVertsPerFace = 8;
        FatVertex faceVertices[cMaxVertsPerFace];
        memset(faceVertices, 0, sizeof(faceVertices));

        // See how many materials we are using, and how many faces in each
        std::vector<uint32_t> materialFaceCounts;
        materialFaceCounts.insert(materialFaceCounts.begin(), materials.size(), 0);
        uint32_t numMaterials = 0;
        for (size_t WhichFace = 0; WhichFace < shapes[WhichShape].mesh.num_face_vertices.size(); WhichFace++)
        {
            // get the per-face material index (copied to output FatVertices) and determine uniqueness
            int faceMaterialId = shapes[WhichShape].mesh.material_ids[WhichFace];
            if (materialFaceCounts[faceMaterialId]++ == 0)
            {
                ++numMaterials;
            }
        }

        // Make a mesh for each material (shape is getting split)
        std::vector<MeshObjectIntermediate*> shapeMaterials;
        shapeMaterials.insert(shapeMaterials.begin(), materials.size(), 0);
        // Ensure the meshObjects vector backing memory does not expand while we do this shape's materials
        meshObjects.reserve(meshObjects.size() + numMaterials);

        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t WhichFace = 0; WhichFace < shapes[WhichShape].mesh.num_face_vertices.size(); WhichFace++)
        {
            const int VertsPerFace = shapes[WhichShape].mesh.num_face_vertices[WhichFace];

            // get the per-face material index and make a new mesh for this material (if we didnt already)
            const int faceMaterialId = shapes[WhichShape].mesh.material_ids[WhichFace];
            if (shapeMaterials[faceMaterialId] == nullptr)
            {
                // MaterialDef has changed to include some new values.  This logic for adding these new values
                // has NOT been tested on OBJ files (do we still even use OBJ?)
                shapeMaterials[faceMaterialId] = &meshObjects.emplace_back();
                shapeMaterials[faceMaterialId]->m_VertexBuffer.reserve(materialFaceCounts[faceMaterialId] * 3/*TODO: could be better!*/);
                shapeMaterials[faceMaterialId]->m_Materials.emplace_back(MaterialDef{
                    materials[faceMaterialId].name,
                    0,  // materialId
                    materials[faceMaterialId].diffuse_texname,
                    glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),              // baseColorFactor
                    materials[faceMaterialId].bump_texname,
                    materials[faceMaterialId].emissive_texname,
                    materials[faceMaterialId].specular_texname,
                    0.5f,                                           // metallicFactor
                    0.5f,                                           // roughnessFactor
                    !materials[faceMaterialId].alpha_texname.empty(),
                    materials[faceMaterialId].illum == 7
                });
            }

            shapeMaterials[faceMaterialId]->m_VertexBuffer.resize(objVerticesSize + VertsPerFace);
            FatVertex* pCurrentFaceVerts = shapeMaterials[faceMaterialId]->m_VertexBuffer.data() + objVerticesSize;
            memset(pCurrentFaceVerts, 0, VertsPerFace * sizeof(FatVertex));
            objVerticesSize += VertsPerFace;

            // Loop over vertices in the face and fill in faceVertices.
            for (size_t WhichVert = 0; WhichVert < VertsPerFace; WhichVert++)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[WhichShape].mesh.indices[index_offset + WhichVert];

                // Fill in the placeholder struct
                pCurrentFaceVerts[WhichVert].position[0] = attrib.vertices[3 * idx.vertex_index + 0];
                pCurrentFaceVerts[WhichVert].position[1] = attrib.vertices[3 * idx.vertex_index + 1];
                pCurrentFaceVerts[WhichVert].position[2] = attrib.vertices[3 * idx.vertex_index + 2];
                pCurrentFaceVerts[WhichVert].normal[0] = attrib.normals[3 * idx.normal_index + 0];
                pCurrentFaceVerts[WhichVert].normal[1] = attrib.normals[3 * idx.normal_index + 1];
                pCurrentFaceVerts[WhichVert].normal[2] = attrib.normals[3 * idx.normal_index + 2];
                pCurrentFaceVerts[WhichVert].uv0[0] = attrib.texcoords[2 * idx.texcoord_index + 0];
                pCurrentFaceVerts[WhichVert].uv0[1] = attrib.texcoords[2 * idx.texcoord_index + 1];
                if (meshHasColor)
                {
                    pCurrentFaceVerts[WhichVert].color[0] = attrib.colors[3 * idx.vertex_index + 0];//red
                    pCurrentFaceVerts[WhichVert].color[1] = attrib.colors[3 * idx.vertex_index + 1];//green
                    pCurrentFaceVerts[WhichVert].color[2] = attrib.colors[3 * idx.vertex_index + 2];//blue
                }
                pCurrentFaceVerts[WhichVert].material = faceMaterialId;
            }

            // Calculate the tangents for the face
            if (VertsPerFace == 3 /* algorithm only works for tris*/)
            {
                glm::vec3 face_tangent, face_bitangent;
                CalculateFaceTangentAndBitangent(pCurrentFaceVerts[0].position, pCurrentFaceVerts[1].position, pCurrentFaceVerts[2].position, pCurrentFaceVerts[0].uv0, pCurrentFaceVerts[1].uv0, pCurrentFaceVerts[2].uv0,
                    face_tangent, face_bitangent);
                // Compute the tangents for each vert on the face
                for (size_t WhichVert = 0; WhichVert < VertsPerFace; WhichVert++)
                {
                    glm::vec3 outTangent;
                    glm::vec3 outBitangent;
                    CalculateTangentAndBitangent(pCurrentFaceVerts[WhichVert].normal, face_tangent, face_bitangent, outTangent, outBitangent);
                    pCurrentFaceVerts[WhichVert].tangent[0] = outTangent.x;
                    pCurrentFaceVerts[WhichVert].tangent[1] = outTangent.y;
                    pCurrentFaceVerts[WhichVert].tangent[2] = outTangent.z;
                    pCurrentFaceVerts[WhichVert].bitangent[0] = outBitangent.x;
                    pCurrentFaceVerts[WhichVert].bitangent[1] = outBitangent.y;
                    pCurrentFaceVerts[WhichVert].bitangent[2] = outBitangent.z;
                }
            }

            index_offset += VertsPerFace;

            // per-face material
            // shapes[WhichShape].mesh.material_ids[WhichFace];
        }   // WhichFace
    }   // WhichShape

    return meshObjects;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<MeshObjectIntermediate> MeshObjectIntermediate::LoadGLTF(AssetManager& assetManager, const std::string& filename, bool ignoreTransforms, const glm::vec3 globalScale)
{
    MeshLoaderModelSceneSanityCheck meshSanityCheckProcessor(filename);
    MeshObjectIntermediateGltfProcessor meshObjectProcessor(filename, ignoreTransforms, globalScale);
    if (!MeshLoader::LoadGltf(assetManager, filename, /*variadic parameter list starts here..*/ meshSanityCheckProcessor, meshObjectProcessor))
    {
        return {};
    }

    return std::move(meshObjectProcessor.m_meshObjects);
}

///////////////////////////////////////////////////////////////////////////////
    
// Builds a screen space mesh as a single Vertex Buffer containing relevant vertex
// positions, normals and threadColors. Vertices are held in TRIANGLE_LIST format.
MeshObjectIntermediate MeshObjectIntermediate::CreateScreenSpaceMesh(glm::vec4 PosLLRadius, glm::vec4 UVLLRadius)
{
    MeshObjectIntermediate mesh;
    mesh.m_VertexBuffer.reserve( 4 );

    FatVertex vert{};
    vert.normal[2] = 1.0f;
    vert.color[0] = 1.0f;
    vert.color[1] = 1.0f;
    vert.color[2] = 1.0f;
    vert.color[3] = 1.0f;

    vert.position[0] = PosLLRadius[0];
    vert.position[1] = PosLLRadius[1];
    vert.uv0[0] = UVLLRadius[0];
    vert.uv0[1] = 1.0f - UVLLRadius[1];
    mesh.m_VertexBuffer.push_back( vert );

    vert.position[0] = PosLLRadius[0] + PosLLRadius[2];
    vert.position[1] = PosLLRadius[1];
    vert.uv0[0] = UVLLRadius[0] + UVLLRadius[2];
    vert.uv0[1] = 1.0f - UVLLRadius[1];
    mesh.m_VertexBuffer.push_back( vert );

    vert.position[0] = PosLLRadius[0];
    vert.position[1] = PosLLRadius[1] + PosLLRadius[3];
    vert.uv0[0] = UVLLRadius[0];
    vert.uv0[1] = 1.0f - (UVLLRadius[1] + UVLLRadius[3]);
    mesh.m_VertexBuffer.push_back( vert );

    vert.position[0] = PosLLRadius[0] + PosLLRadius[2];
    vert.position[1] = PosLLRadius[1] + PosLLRadius[3];
    vert.uv0[0] = UVLLRadius[0] + UVLLRadius[2];
    vert.uv0[1] = 1.0f - (UVLLRadius[1] + UVLLRadius[3]);
    mesh.m_VertexBuffer.push_back( vert );

    const uint16_t indices[6] = { 0,1,2, 1,3,2 };
    mesh.m_IndexBuffer.emplace<std::vector<uint16_t>>( std::begin(indices), std::end(indices) );

    return mesh;
}

///////////////////////////////////////////////////////////////////////////////

// Builds a screen space mesh as a single Vertex Buffer containing vertices for a
// single triangle that covers the entire screen.  UVs, Colors, Normals are all
// zero (not intended to be used!)
///////////////////////////////////////////////////////////////////////////////

MeshObjectIntermediate MeshObjectIntermediate::CreateScreenSpaceTriMesh()
{
    MeshObjectIntermediate mesh;
    mesh.m_VertexBuffer.reserve( 3 );

    FatVertex vert {};

    vert.position[0] = -1.0f;
    vert.position[1] = 1.0f;
    mesh.m_VertexBuffer.push_back( vert );

    vert.position[0] = 3.0f;
    vert.position[1] = 1.0f;
    mesh.m_VertexBuffer.push_back( vert );

    vert.position[0] = -1.0f;
    vert.position[1] = -3.0f;
    mesh.m_VertexBuffer.push_back( vert );

    return mesh;
}

///////////////////////////////////////////////////////////////////////////////

// String literal lower case hash (constexpr)
constexpr FnvHashLower operator "" _h(const char* str, size_t) { return FnvHashLower(str); }

std::vector<uint32_t> MeshObjectIntermediate::CopyFatVertexToFormattedBuffer(const std::span<const MeshObjectIntermediate::FatVertex>& fatVertexBuffer, const VertexFormat& vertexFormat)
{
    //
    // Determine the arrangement of the output data (based on the vertexFormat)
    // Uses case insensitive slot/semantic name comparision.
    //
    int positionIndex = -1;
    int uv0Index = -1;
    int normalIndex = -1;
    int colorIndex = -1;
    int tangentIndex = -1;
    int bitangentIndex = -1;

    for (int i = 0; i < vertexFormat.elementIds.size(); ++i)
    {
        const std::string& elementId = vertexFormat.elementIds[i];
        const FnvHashLower elementHash(elementId);   // use hashes to compare, all the hashing in the switch statement is compile time!

        switch (elementHash) {
        case "Position"_h:
            positionIndex = i;
            break;
        case "UV"_h:
        case "TEXCOORD"_h:
            uv0Index = i;
            break;
        case "Color"_h:
            colorIndex = i;
            break;
        case "Normal"_h:
            normalIndex = i;
            break;
        case "Tangent"_h:
            tangentIndex = i;
            break;
        case "Bitangent"_h:
            bitangentIndex = i;
            break;
        default:
            LOGE("Cannot map vertex elementId %s to the mesh data", elementId.c_str());
            break;
        }
    }

    const uint32_t destSpan32 = (uint32_t)vertexFormat.span / 4;  // span of output data (in 32bit words)
    assert((vertexFormat.span & 3) == 0);   // does not support spans that are not a multiple of 4

    // Determine the mapping between the FatVertex (input) data and the output VertexFormat items
    std::array<int, sizeof(MeshObjectIntermediate::FatVertex) / sizeof(float)> copyOffsets;
    memset(copyOffsets.data(), -1, sizeof(copyOffsets));
    for (const std::pair<uint32_t, int>& srcOffset_DestIndex : {
        std::pair<uint32_t, int>((uint32_t)offsetof(MeshObjectIntermediate::FatVertex, position) / 4, positionIndex),
        std::pair<uint32_t, int>((uint32_t)offsetof(MeshObjectIntermediate::FatVertex, normal) / 4, normalIndex),
        std::pair<uint32_t, int>((uint32_t)offsetof(MeshObjectIntermediate::FatVertex, color) / 4, colorIndex),
        std::pair<uint32_t, int>((uint32_t)offsetof(MeshObjectIntermediate::FatVertex, uv0) / 4, uv0Index),
        std::pair<uint32_t, int>((uint32_t)offsetof(MeshObjectIntermediate::FatVertex, tangent) / 4, tangentIndex),
        std::pair<uint32_t, int>((uint32_t)offsetof(MeshObjectIntermediate::FatVertex, bitangent) / 4, bitangentIndex) })
    {
        int destIndex = srcOffset_DestIndex.second;
        if (destIndex != -1)
        {
            uint32_t srcOffset32 = srcOffset_DestIndex.first;
            uint32_t destOffset = vertexFormat.elements[destIndex].offset;
            assert((destOffset & 3) == 0);      // cannot handle non 4 byte aligned data
            destOffset /= 4;
            const uint32_t destSize = vertexFormat.elements[destIndex].type.size();
            switch (destSize)
            {
            case 16:
                copyOffsets[srcOffset32++] = destOffset++;
            case 12:
                copyOffsets[srcOffset32++] = destOffset++;
            case 8:
                copyOffsets[srcOffset32++] = destOffset++;
            case 4:
                copyOffsets[srcOffset32++] = destOffset++;
                break;
            default:
                assert(0);
                break;
            }
        }
    }

    const size_t numVertices = fatVertexBuffer.size();

    std::vector<uint32_t> outputData;
    outputData.resize(destSpan32 * numVertices, 0/*zero buffer*/);

    uint32_t* pSrc = (uint32_t*)fatVertexBuffer.data();
    uint32_t* pDst = outputData.data();

    for (size_t i = 0; i < numVertices; ++i)
    {
        // Copy from tinyObjVertex to our buffer vertex format (compiler may decide to unroll this since copyOffsets.size() is known at compile time
        for (uint32_t srcOffset = 0; srcOffset < copyOffsets.size(); ++srcOffset)
        {
            if (copyOffsets[srcOffset] >= 0)
            {
                pDst[copyOffsets[srcOffset]] = *pSrc;
            }
            ++pSrc;
        }
        pDst += destSpan32;
    }   // WhichVert

    return outputData;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> MeshObjectIntermediate::CopyFatInstanceToFormattedBuffer(const std::span<const MeshObjectIntermediate::FatInstance>& fatInstanceBuffer, const VertexFormat& format)
{
    //
    // Determine the arrangement of the output data (based on the vertexFormat)
    //
    int transformIndex = -1;
    int transform0Index = -1;
    int transform1Index = -1;
    int transform2Index = -1;
    int nodeIdIndex = -1;
    for (int i = 0; i < format.elementIds.size(); ++i)
    {
        const std::string& elementId = format.elementIds[i];
        if (elementId == "Transform")
        {
            transformIndex = i;
        }
        else if (elementId == "Transform0")
        {
            transform0Index = i;
        }
        else if (elementId == "Transform1")
        {
            transform1Index = i;
        }
        else if (elementId == "Transform2")
        {
            transform2Index = i;
        }
        else if (elementId == "NodeId")
        {
            nodeIdIndex = i;
        }
        else
        {
            LOGE("Cannot map instance elementId %s to the mesh data", elementId.c_str());
        }
    }

    const uint32_t destSpan32 = (uint32_t)format.span / 4;  // span of output data (in 32bit words)
    assert((format.span & 3) == 0);   // does not support spans that are not a multiple of 4

    // Determine the mapping between the FatInstance (input) data and the output VertexFormat items
    std::array<int, sizeof(MeshObjectIntermediate::FatInstance) / sizeof(float)> copyOffsets;
    memset(copyOffsets.data(), -1, sizeof(copyOffsets));
    for (const std::pair<uint32_t, int>& srcOffset_DestIndex : {
        std::pair<uint32_t, int>((uint32_t)offsetof(MeshObjectIntermediate::FatInstance, transform) / 4, transformIndex),
        std::pair<uint32_t, int>(0 + (uint32_t)offsetof(MeshObjectIntermediate::FatInstance, transform) / 4, transform0Index),
        std::pair<uint32_t, int>(4 + (uint32_t)offsetof(MeshObjectIntermediate::FatInstance, transform) / 4, transform1Index),
        std::pair<uint32_t, int>(8 + (uint32_t)offsetof(MeshObjectIntermediate::FatInstance, transform) / 4, transform2Index),
        std::pair<uint32_t, int>((uint32_t)offsetof(MeshObjectIntermediate::FatInstance, nodeId) / 4, nodeIdIndex) })
    {
        int destIndex = srcOffset_DestIndex.second;
        if (destIndex != -1)
        {
            uint32_t srcOffset32 = srcOffset_DestIndex.first;
            uint32_t destOffset = format.elements[destIndex].offset;
            assert((destOffset & 3) == 0);      // cannot handle non 4 byte aligned data
            destOffset /= 4;
            const uint32_t destSize = format.elements[destIndex].type.size();
            switch (destSize)
            {
            case 16:
                copyOffsets[srcOffset32++] = destOffset++;
            case 12:
                copyOffsets[srcOffset32++] = destOffset++;
            case 8:
                copyOffsets[srcOffset32++] = destOffset++;
            case 4:
                copyOffsets[srcOffset32++] = destOffset++;
                break;
            default:
                assert(0);
                break;
            }
        }
    }

    const size_t numInstances = fatInstanceBuffer.size();

    std::vector<uint32_t> outputData;
    outputData.resize(destSpan32 * numInstances, 0/*zero buffer*/);

    uint32_t* pSrc = (uint32_t*)fatInstanceBuffer.data();
    uint32_t* pDst = outputData.data();

    for (size_t i = 0; i < numInstances; ++i)
    {
        // Copy from tinyObjVertex to our buffer vertex format (compiler may decide to unroll this since copyOffsets.size() is known at compile time
        for (uint32_t srcOffset = 0; srcOffset < copyOffsets.size(); ++srcOffset)
        {
            if (copyOffsets[srcOffset] >= 0)
            {
                pDst[copyOffsets[srcOffset]] = *pSrc;
            }
            ++pSrc;
        }
        pDst += destSpan32;
    }   // WhichVert

    return outputData;
}

///////////////////////////////////////////////////////////////////////////////

size_t MeshObjectIntermediate::CalcNumTriangles() const
{
    return std::visit( [&]( const auto& m )
        {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, std::vector<uint32_t>> || std::is_same_v<T, std::vector<uint16_t>>)
            {
                return m.size() / 3;
            }
            else
            {
                return m_VertexBuffer.size() / 3;;
            }
        }, this->m_IndexBuffer );
}

///////////////////////////////////////////////////////////////////////////////

/*static*/ std::vector<std::string> MeshObjectIntermediate::ExtractTextureNames(const std::vector<MeshObjectIntermediate>& meshObjects)
{
    std::set<std::string> textureNames;
    for (const auto& object : meshObjects)
    {
        for (const auto& material : object.m_Materials)
        {
            if (!material.diffuseFilename.empty())
                textureNames.insert(material.diffuseFilename);
            if (!material.bumpFilename.empty())
                textureNames.insert(material.bumpFilename);
        }
    }
    std::vector<std::string> textureNamesVector;
    textureNamesVector.reserve(textureNames.size());
    std::copy(textureNames.begin(), textureNames.end(), std::back_inserter(textureNamesVector));
    return textureNamesVector;
}


///////////////////////////////////////////////////////////////////////////////

bool MeshObjectIntermediateGltfProcessor::operator()(const tinygltf::Model& ModelData)
{
    const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];

    // Get a count of the total number of mesh object we wll be outputting (so we can pre-size the vector).
    size_t totalPrimitives = 0;
    auto& meshObjects = m_meshObjects;

    // Go through all the scene nodes, counting the number of primitives
    bool success = MeshLoader::RecurseModelNodes(ModelData, SceneData.nodes, [&totalPrimitives, this](const tinygltf::Model& ModelData, const MeshLoader::NodeTransform&, const tinygltf::Node& NodeData) -> bool {
        if (NodeData.mesh >= 0)
        {
            if (NodeData.mesh >= ModelData.meshes.size())
            {
                printf("\nError loading %s: SkeletonNodeData mesh is invalid index", m_filename.c_str());
                return false;
            }
            const tinygltf::Mesh& MeshData = ModelData.meshes[NodeData.mesh];
            totalPrimitives += MeshData.primitives.size();
        }
        return true;
        });
    if (!success)
        return false;

    // Pre-reserve the vector size so as not to incur the cost of constantly re-allocating and moving mesh data.
    meshObjects.reserve(totalPrimitives);

    // Go through all the scene nodes, outputting models for each
    return MeshLoader::RecurseModelNodes(ModelData, SceneData.nodes, [this, &meshObjects](const tinygltf::Model& ModelData, const MeshLoader::NodeTransform& Transform, const tinygltf::Node& NodeData) -> bool {
        if (NodeData.mesh >= 0)
        {
            // ... get the mesh for this node ...
            const tinygltf::Mesh& MeshData = ModelData.meshes[NodeData.mesh];

            size_t NodeIdx = &NodeData - &ModelData.nodes[0];
            if (NodeIdx < 0 || NodeIdx >= ModelData.nodes.size())
                NodeIdx = -1;

            for (uint32_t WhichPrimitive = 0; WhichPrimitive < MeshData.primitives.size(); ++WhichPrimitive)
            {
                const tinygltf::Primitive& PrimitiveData = MeshData.primitives[WhichPrimitive];
                if (PrimitiveData.mode != TINYGLTF_MODE_TRIANGLES)
                {
                    // we dont handle anything other than triangles currently.
                    continue;
                }

                gltfAttribInfo AttribInfo[NUM_GLTF_ATTRIBS];

                // Indices are not "parsed" but can be accessed directly
                AttribInfo[ATTRIB_INDICES].AccessorIndx = PrimitiveData.indices;

                std::map<std::string, int>::const_iterator AttribIter;
                for (AttribIter = PrimitiveData.attributes.begin(); AttribIter != PrimitiveData.attributes.end(); AttribIter++)
                {
                    if (AttribIter->first.compare("POSITION") == 0)
                        AttribInfo[ATTRIB_POSITION].AccessorIndx = AttribIter->second;
                    else if (AttribIter->first.compare("NORMAL") == 0)
                        AttribInfo[ATTRIB_NORMAL].AccessorIndx = AttribIter->second;
                    else if (AttribIter->first.compare("TANGENT") == 0)
                        AttribInfo[ATTRIB_TANGENT].AccessorIndx = AttribIter->second;
                    else if (AttribIter->first.compare("TEXCOORD_0") == 0)
                        AttribInfo[ATTRIB_TEXCOORD_0].AccessorIndx = AttribIter->second;
                    else if (AttribIter->first.compare("TEXCOORD_1") == 0)
                        AttribInfo[ATTRIB_TEXCOORD_1].AccessorIndx = AttribIter->second;
                    else if (AttribIter->first.compare("COLOR_0") == 0)
                        AttribInfo[ATTRIB_COLOR_0].AccessorIndx = AttribIter->second;
                    else if (AttribIter->first.compare("JOINTS_0") == 0)
                        AttribInfo[ATTRIB_JOINTS_0].AccessorIndx = AttribIter->second;
                    else if (AttribIter->first.compare("WEIGHTS_0") == 0)
                        AttribInfo[ATTRIB_WEIGHTS_0].AccessorIndx = AttribIter->second;
                }

                // Need to have at least Indices and position
                if (AttribInfo[ATTRIB_INDICES].AccessorIndx < 0)
                {
                    printf("\nError loading %s: Mesh has no indices", m_filename.c_str());
                    return false;
                }
                if (AttribInfo[ATTRIB_POSITION].AccessorIndx < 0)
                {
                    printf("\nError loading %s: Mesh has no position data", m_filename.c_str());
                    return false;
                }


                // We now know the  ModelData.accessors[] index for all our data.
                for (uint32_t WhichAttrib = 0; WhichAttrib < NUM_GLTF_ATTRIBS; WhichAttrib++)
                {
                    if (AttribInfo[WhichAttrib].AccessorIndx >= 0)
                    {
                        const tinygltf::Accessor& AccessorData = ModelData.accessors[AttribInfo[WhichAttrib].AccessorIndx];
                        const tinygltf::BufferView& ViewData = ModelData.bufferViews[AccessorData.bufferView];
                        int Stride = AccessorData.ByteStride(ViewData);
                        if (Stride < 0)
                        {
                            printf("\nError loading %s: Cannot calculate data stride", m_filename.c_str());
                            return false;
                        }
                        AttribInfo[WhichAttrib].BytesPerElem = Stride;
                        AttribInfo[WhichAttrib].BytesTotal = ViewData.byteLength;
                        AttribInfo[WhichAttrib].Count = (uint32_t) AccessorData.count;

                        const tinygltf::Buffer& BufferData = ModelData.buffers[ViewData.buffer];
                        AttribInfo[WhichAttrib].pData = (void*)(&BufferData.data.at(ViewData.byteOffset + AccessorData.byteOffset));
                    }
                }

                if (AttribInfo[ATTRIB_TEXCOORD_0].Count > 0 && AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem != 8)
                {
                    // Do we handle texture coordinates that are not UV?
                    printf("\nError loading %s: Texture coordinates are not UV only", m_filename.c_str());
                    return false;
                }

                // Paranoia Check that same number of positions, normals, and texture coordinates
                uint32_t NumIndices = AttribInfo[ATTRIB_INDICES].Count;
                uint32_t NumPositions = AttribInfo[ATTRIB_POSITION].Count;
                uint32_t NumNormals = AttribInfo[ATTRIB_NORMAL].Count;
                uint32_t NumTexCoords = AttribInfo[ATTRIB_TEXCOORD_0].Count;

                if (NumNormals > 0 && NumNormals != NumPositions)
                {
                    printf("\nError loading %s: Mesh has different number of positions and normals", m_filename.c_str());
                    return false;
                }

                if (NumTexCoords > 0 && NumTexCoords != NumPositions)
                {
                    printf("\nError loading %s: Mesh has different number of positions and texture coordinates", m_filename.c_str());
                    return false;
                }

                // Finally, we can fill in the actual mesh data
                // Comment out since large scenes spam log file
                // LOGI("    Mesh Object:");
                // LOGI("      %d Indices (%d triangles)", NumIndices, NumIndices / 3);
                // LOGI("      %d Positions", NumPositions);
                // LOGI("      %d Normals", NumNormals);
                // LOGI("      %d UVs", NumTexCoords);

                int materialIdx = PrimitiveData.material;

                // Add the (empty) object to the meshObject vector.
                auto& meshObject = meshObjects.emplace_back();

                // Pass up the mesh name 
                meshObject.m_MeshName = MeshData.name;

                // Set the object transform.
                meshObject.m_Transform = m_ignoreTransforms ? glm::mat4{1.0f} : Transform;
                meshObject.m_Transform[3] *= glm::vec4(m_globalScale, 1.0f);// Transform position needs scale applying, dont scale entire transform as the vertex data is scaled independantly (below).
                meshObject.m_NodeId = (int)NodeIdx;

                if (materialIdx >= 0)/*-1 is valid*/
                {
                    // Was seeing invalid material index on some GLTF exports.  Protect from that.
                    if (materialIdx >= ModelData.materials.size())
                    {
                        LOGE("Mesh referenced invalid material (Material %d; Max Available = %zu)!  Defaulting to first material in list.", materialIdx + 1, ModelData.materials.size());
                        materialIdx = 0;
                    }

                    // Pull out the relevant material information.
                    const auto& material = ModelData.materials[materialIdx];

                    int baseColorTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                    baseColorTextureIndex = baseColorTextureIndex >= 0 ? ModelData.textures[baseColorTextureIndex].source : -1;

                    glm::vec4 baseColorFactor = glm::vec4(material.pbrMetallicRoughness.baseColorFactor[0],
                                                            material.pbrMetallicRoughness.baseColorFactor[1],
                                                            material.pbrMetallicRoughness.baseColorFactor[2],
                                                            material.pbrMetallicRoughness.baseColorFactor[3]);

                    int normalIndex = material.normalTexture.index;
                    normalIndex = normalIndex >= 0 ? ModelData.textures[normalIndex].source : -1;

                    int emissiveIndex = material.emissiveTexture.index;
                    emissiveIndex = emissiveIndex >= 0 ? ModelData.textures[emissiveIndex].source : -1;

                    int pbrIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
                    pbrIndex = pbrIndex >= 0 ? ModelData.textures[pbrIndex].source : -1;

                    meshObject.m_Materials.emplace_back(MeshObjectIntermediate::MaterialDef{
                        material.name,
                        0,  // materialId

                        baseColorTextureIndex >= 0 ? ModelData.images[baseColorTextureIndex].uri : "",
                        baseColorFactor,

                        normalIndex >= 0 ? ModelData.images[normalIndex].uri : "",
                        emissiveIndex >= 0 ? ModelData.images[emissiveIndex].uri : "",

                        pbrIndex >= 0 ? ModelData.images[pbrIndex].uri : "",
                        (float)material.pbrMetallicRoughness.metallicFactor,

                        (float)material.pbrMetallicRoughness.roughnessFactor,

                        material.alphaMode == "MASK",
                        material.alphaMode == "BLEND" });

                    materialIdx = (int) meshObject.m_Materials.size() - 1;  // re-patch the materialIdx to reference the index within this meshObject's materials
                }
                meshObject.m_VertexBuffer.reserve(NumPositions);

                // Copy over the index buffer data.
                if (AttribInfo[ATTRIB_INDICES].BytesPerElem == 4)
                {
                    uint32_t* p32 = (uint32_t*)AttribInfo[ATTRIB_INDICES].pData;
                    std::span<uint32_t> indicesSpan{ p32, NumIndices };
                    meshObject.m_IndexBuffer.emplace< std::vector<uint32_t>>(indicesSpan.begin(), indicesSpan.end());
                }
                else if (AttribInfo[ATTRIB_INDICES].BytesPerElem == 2)
                {
                    uint16_t* p16 = (uint16_t*)AttribInfo[ATTRIB_INDICES].pData;
                    std::span<uint16_t> indicesSpan{ p16, NumIndices };
                    meshObject.m_IndexBuffer.emplace< std::vector<uint16_t>>(indicesSpan.begin(), indicesSpan.end());
                }
                else
                {
                    printf("\nError loading %s: Mesh has invalid BytesPerElem for indices", m_filename.c_str());
                    return false;
                }

                float* pPosition = (float*)AttribInfo[ATTRIB_POSITION].pData;
                uint32_t positionIncr = AttribInfo[ATTRIB_POSITION].BytesPerElem / sizeof(float);
                float* pNormals = (float*)AttribInfo[ATTRIB_NORMAL].pData;
                uint32_t normalIncr = AttribInfo[ATTRIB_NORMAL].BytesPerElem / sizeof(float);
                float* pTangents = (float*)AttribInfo[ATTRIB_TANGENT].pData;
                uint32_t tangentIncr = AttribInfo[ATTRIB_TANGENT].BytesPerElem / sizeof(float);
                float* pTexCoords = (float*)AttribInfo[ATTRIB_TEXCOORD_0].pData;
                uint32_t texCoordsIncr = AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem / sizeof(float);

                // The problem is that color can come in as unsigned short (not floats), so 
                // can't access as a float pointer
                // float* pColor = (float*)AttribInfo[ATTRIB_COLOR_0].pData;
                // uint32_t colorIncr = AttribInfo[ATTRIB_COLOR_0].BytesPerElem / sizeof(float);

                for (uint32_t WhichVert = 0; WhichVert < NumPositions; ++WhichVert)
                {
                    MeshObjectIntermediate::FatVertex vertex = {};
                    if (pPosition != nullptr)
                    {
                        vertex.position[0] = pPosition[0] * m_globalScale.x;
                        vertex.position[1] = pPosition[1] * m_globalScale.y;
                        vertex.position[2] = pPosition[2] * m_globalScale.z;
                        pPosition += positionIncr;
                    }

                    if (pNormals != nullptr)
                    {
                        vertex.normal[0] = pNormals[0];
                        vertex.normal[1] = pNormals[1];
                        vertex.normal[2] = pNormals[2];
                        pNormals += normalIncr;
                    }
                    else
                    {
                        vertex.normal[0] = 0.0f;
                        vertex.normal[1] = 0.0f;
                        vertex.normal[2] = 1.0f;
                    }

                    if (pTangents != nullptr)
                    {
                        vertex.tangent[0] = pTangents[0];
                        vertex.tangent[1] = pTangents[1];
                        vertex.tangent[2] = pTangents[2];
                        pTangents += tangentIncr;
                    }
                    else
                    {
                        vertex.tangent[0] = 1.0f;
                        vertex.tangent[1] = 0.0f;
                        vertex.tangent[2] = 0.0f;
                    }

                    if (pTexCoords != nullptr)
                    {
                        vertex.uv0[0] = pTexCoords[0];
                        vertex.uv0[1] = pTexCoords[1];
                        pTexCoords += texCoordsIncr;
                    }

                    // Default vertice color is white (debug with pink if needed)
                    if (AttribInfo[ATTRIB_COLOR_0].pData != nullptr)
                    {
                        if (AttribInfo[ATTRIB_COLOR_0].BytesPerElem == 8)
                        {
                            // Data is UNSIGNED_SHORT
                            uint16_t R, G, B, A;
                            uint16_t* pUShortColor = (uint16_t*)AttribInfo[ATTRIB_COLOR_0].pData;

                            R = pUShortColor[WhichVert * 4 + 0];
                            G = pUShortColor[WhichVert * 4 + 1];
                            B = pUShortColor[WhichVert * 4 + 2];
                            A = pUShortColor[WhichVert * 4 + 3];

                            // Convert from USHORT to FLOAT
                            vertex.color[0] = (float)R / 65535.0f;
                            vertex.color[1] = (float)G / 65535.0f;
                            vertex.color[2] = (float)B / 65535.0f;
                            vertex.color[3] = (float)A / 65535.0f;

                        }
                        else if (AttribInfo[ATTRIB_COLOR_0].BytesPerElem == 16)
                        {
                            // Data is FLOAT?
                            float* pFloatColor = (float*)AttribInfo[ATTRIB_COLOR_0].pData;

                            vertex.color[0] = pFloatColor[WhichVert * 4 + 0];
                            vertex.color[1] = pFloatColor[WhichVert * 4 + 1];
                            vertex.color[2] = pFloatColor[WhichVert * 4 + 2];
                            vertex.color[3] = pFloatColor[WhichVert * 4 + 3];
                        }
                        else
                        {
                            printf("\nError loading %s: Mesh has invalid BytesPerElem (%d) for color", m_filename.c_str(), AttribInfo[ATTRIB_COLOR_0].BytesPerElem);
                            return false;
                        }
                    }
                    else
                    {
                        vertex.color[0] = 1.0f;
                        vertex.color[1] = 1.0f;
                        vertex.color[2] = 1.0f;
                        vertex.color[3] = 1.0f;
                    }

                    if (AttribInfo[ATTRIB_NORMAL].pData != nullptr)
                    {
                        glm::vec3 bitangent = {};
                        if (AttribInfo[ATTRIB_TANGENT].pData != nullptr)
                        {
                            bitangent = glm::cross(glm::vec3{ vertex.normal[0], vertex.normal[1], vertex.normal[2] }, glm::vec3{ vertex.tangent[0], vertex.tangent[1], vertex.tangent[2] });
                        }
                        else
                        {
                            bitangent[0] = 0.0f;
                            bitangent[1] = 1.0f;
                            bitangent[2] = 0.0f;
                        }
                        vertex.bitangent[0] = bitangent[0];
                        vertex.bitangent[1] = bitangent[1];
                        vertex.bitangent[2] = bitangent[2];
                    }

                    vertex.material = materialIdx;
                    meshObject.m_VertexBuffer.push_back(vertex);
                }
            }
        }
        return true;
        });
}

///////////////////////////////////////////////////////////////////////////////

void to_json( Json& j, const MeshObjectIntermediate::MaterialDef& material )
{
    j = Json { {"MaterialId", material.materialId},
               {"Diffuse", material.diffuseFilename},
               {"Bump", material.bumpFilename},
               {"Emissive", material.emissiveFilename},
               {"SpecMap", material.specMapFilename},
               {"AlphaCutout", material.alphaCutout},
               {"Transparent", material.transparent},
    };
}

///////////////////////////////////////////////////////////////////////////////

void from_json( const nlohmann::json& j, MeshObjectIntermediate::MaterialDef& material )
{
    material = {};
    material.materialId = j.value("MaterialId", uint32_t(0));
    j.at( "Diffuse" ).get_to( material.diffuseFilename );
    j.at( "Bump" ).get_to( material.bumpFilename );
    j.at( "Emissive" ).get_to( material.emissiveFilename );
    j.at( "SpecMap" ).get_to( material.specMapFilename );
    j.at( "AlphaCutout" ).get_to( material.alphaCutout );
    j.at( "Transparent" ).get_to( material.transparent );
}
