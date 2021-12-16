// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "tinyobjloader/tiny_obj_loader.h"
#include "MeshObject.h"
#include "material/vertexFormat.hpp"
#include "system/os_common.h"
#include "system/assetManager.hpp"
#include "system/glm_common.hpp"
#include "memory/vertexBufferObject.hpp"
#include <istream>
#include <sstream>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRIE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define JSON_NOEXCEPTION
#include "tinygltf/tiny_gltf.h"

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
     }
};

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
    int32_t         AccessorIndx;

    // Number of bytes per element
    uint32_t        BytesPerElem;

    // Total number of bytes
    uint32_t        BytesTotal;

    // Pointer to data within the glTF buffer
    void* pData;

} gltfAttribInfo;


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

// Loads a .obj and .mtl file and builds a single Vertex Buffer containing relevant vertex
// positions, normals and threadColors. Vertices are held in TRIANGLE_LIST format.
//
// Vertex layout corresponds to MeshObject::vertex_layout structure.
bool MeshObject::LoadObj(Vulkan* pVulkan, AssetManager& assetManager, const std::string& filename, uint32_t binding, MeshObject* meshObject)
{
    // Destroy object before re-creating
    meshObject->Destroy();

    tinyobj::attrib_t                   attrib;
    std::vector<tinyobj::shape_t>       shapes;
    std::vector<tinyobj::material_t>    materials;

    std::string warn;
    std::string err;

    // Load the obj file in to memory.
    AssetMemStream<char> objFile;
    if (!assetManager.LoadFileIntoMemory(filename, objFile))
    {
        LOGE("tinyobj failed to open: %s", filename.c_str());
        return false;
    }

    // Create a stream from the loaded string data.
    MaterialFileReader matFileReader(assetManager, filename);

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &objFile, &matFileReader);
    objFile.clear();

    if (!err.empty())
    {
        LOGE("tinyobj error (%s) loading %s", err.c_str(), filename.c_str());
        return false;
    }

    if (!ret)
    {
        LOGE("tinyobj failed to load %s", filename.c_str());
        return false;
    }

    std::vector<vertex_layout> objVertices;
    LOGI("Mesh object %s has %d Shape[s]", filename.c_str(), (int)shapes.size());
    if (shapes.size() > 1)
    {
        LOGE("    Mesh loader only supports one shape per file!");
        return false;
    }

    // Loop over shapes
    for (size_t WhichShape = 0; WhichShape < shapes.size(); WhichShape++)
    {
        uint32_t NumIndices = (uint32_t)shapes[WhichShape].mesh.indices.size();

        LOGI("    Shape %d:", (int)WhichShape);
        LOGI("      %d Indices (%d triangles)", NumIndices, NumIndices / 3);
        LOGI("      %d Positions", (int)attrib.vertices.size() / 3);
        LOGI("      %d Normals", (int)attrib.normals.size() / 3);
        LOGI("      %d UVs", (int)attrib.texcoords.size() / 2);

        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t WhichFace = 0; WhichFace < shapes[WhichShape].mesh.num_face_vertices.size(); WhichFace++)
        {

            int VertsPerFace = shapes[WhichShape].mesh.num_face_vertices[WhichFace];

            // Loop over vertices in the face.
            for (size_t WhichVert = 0; WhichVert < VertsPerFace; WhichVert++)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[WhichShape].mesh.indices[index_offset + WhichVert];
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
                tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];
                // Optional: vertex colors
                // tinyobj::real_t red = attrib.colors[3*idx.vertex_index+0];
                // tinyobj::real_t green = attrib.colors[3*idx.vertex_index+1];
                // tinyobj::real_t blue = attrib.colors[3*idx.vertex_index+2];

                vertex_layout vertex;
                vertex.pos[0] = vx;
                vertex.pos[1] = vy;
                vertex.pos[2] = vz;

                vertex.normal[0] = nx;
                vertex.normal[1] = ny;
                vertex.normal[2] = nz;

                vertex.uv[0] = tx;
                vertex.uv[1] = ty;

                // Default vertice color is white (debug with pink if needed)
                vertex.color[0] = 1.0f;
                vertex.color[1] = 1.0f;
                vertex.color[2] = 1.0f;
                vertex.color[3] = 1.0f;

                // No tangents in OBJ files :(
                vertex.tangent[0] = 0.0f;
                vertex.tangent[1] = 1.0f;
                vertex.tangent[2] = 0.0f;

                objVertices.push_back(vertex);
            }   // WhichVert
            index_offset += VertsPerFace;

            // per-face material
            // shapes[WhichShape].mesh.material_ids[WhichFace];
        }   // WhichFace

    }   // WhichShape

    meshObject->m_NumVertices = (uint32_t)objVertices.size();
    meshObject->m_VertexBuffers.resize(1);
    meshObject->m_VertexBuffers[0].Initialize(&pVulkan->GetMemoryManager(),
                                          objVertices.size(),
                                          &objVertices[0]);

    meshObject->m_VertexBuffers[0].AddBindingAndAtributes(binding, vertex_layout::sFormat);

    return true;
}

///////////////////////////////////////////////////////////////////////////////

static bool Gltf_FileExists(const std::string& abs_filename, void* vAssetManager)
{
    //AssetManager* pAssetManager = (AssetManager*)vAssetManager;
    return true;
}
static std::string Gltf_ExpandFilePath(const std::string& filename, void* vAssetManager)
{
    return filename;
}
static bool Gltf_ReadWholeFile(std::vector<unsigned char>* output, std::string* error, const std::string& filename, void* vAssetManager)
{
    AssetManager* pAssetManager = (AssetManager*)vAssetManager;
    return pAssetManager->LoadFileIntoMemory(filename, *output);
}
static bool Gltf_WriteWholeFile(std::string*, const std::string&, const std::vector<unsigned char>&, void*)
{
    return false;
}

///////////////////////////////////////////////////////////////////////////////

// Loads a ..gltf file and builds a single Vertex Buffer containing relevant vertex
// positions, normals and threadColors. Vertices are held in TRIANGLE_LIST format.
//
// Vertex layout corresponds to MeshObject::vertex_layout structure.
bool MeshObject::LoadGLTF(Vulkan* pVulkan, AssetManager& assetManager, const std::string& filename, uint32_t binding, MeshObject* meshObject)
{
    tinygltf::Model ModelData;

    std::string err;
    std::string warn;

    gltfAttribInfo  AttribInfo[NUM_GLTF_ATTRIBS];
    for (uint32_t WhichAttrib = 0; WhichAttrib < NUM_GLTF_ATTRIBS; WhichAttrib++)
    {
        AttribInfo[WhichAttrib].AccessorIndx = -1;
        AttribInfo[WhichAttrib].BytesPerElem = 0;
        AttribInfo[WhichAttrib].BytesTotal = 0;
        AttribInfo[WhichAttrib].pData = nullptr;
    }

    {
        tinygltf::TinyGLTF ModelLoader;
        ModelLoader.SetFsCallbacks(tinygltf::FsCallbacks{ &Gltf_FileExists, &Gltf_ExpandFilePath, &Gltf_ReadWholeFile, &Gltf_WriteWholeFile, &assetManager });

        bool RetVal = ModelLoader.LoadASCIIFromFile(&ModelData, &err, &warn, filename);
        if (!warn.empty())
        {
            printf("\nWarning loading %s: %s", filename.c_str(), warn.c_str());
        }

        if (!err.empty())
        {
            printf("\nError loading %s: %s", filename.c_str(), err.c_str());
        }

        if (!RetVal)
        {
            // Assume the error was logged above
            return false;
        }
    }

    // Start with the "Scene"...
    if (ModelData.scenes.size() == 0)
    {
        printf("\nError loading %s: File did not contain a scene node", filename.c_str());
        return false;
    }
    if(ModelData.defaultScene >= ModelData.scenes.size())
    {
        printf("\nError loading %s: Default scene node is out of range", filename.c_str());
        return false;
    }

    const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];

    // ... find the node used by this scene ...
    uint32_t NumNodes = (uint32_t)SceneData.nodes.size();
    if (NumNodes == 0)
    {
        printf("\nError loading %s: Default scene does not contain any nodes", filename.c_str());
        return false;
    }
    if(SceneData.nodes[0] >= ModelData.nodes.size())
    {
        printf("\nError loading %s: Default scene node is invalid index", filename.c_str());
        return false;
    }

    // ... get the node used by this scene ...
    const tinygltf::Node& NodeData = ModelData.nodes[SceneData.nodes[0]];
    if (NodeData.mesh >= ModelData.meshes.size())
    {
        printf("\nError loading %s: Node mesh is invalid index", filename.c_str());
        return false;
    }

    const tinygltf::Mesh& MeshData = ModelData.meshes[NodeData.mesh];

    // Only support one primitive per mesh
    if (MeshData.primitives.size() > 1)
    {
        printf("\nError loading %s: Mesh has more than one primitive", filename.c_str());
        return false;
    }

    const tinygltf::Primitive& PrimitiveData = MeshData.primitives[0];

    uint32_t NumAttribs = (uint32_t)PrimitiveData.attributes.size();

    // Indices are not "parsed" but can be accessed directly
    AttribInfo[ATTRIB_INDICES].AccessorIndx = PrimitiveData.indices;

    std::map<std::string, int>::const_iterator AttribIter;
    for (AttribIter = PrimitiveData.attributes.begin(); AttribIter != PrimitiveData.attributes.end(); AttribIter++)
    {
        if (AttribIter->first.compare("POSITION") == 0)
            AttribInfo[ATTRIB_POSITION].AccessorIndx = AttribIter->second;

        if (AttribIter->first.compare("NORMAL") == 0)
            AttribInfo[ATTRIB_NORMAL].AccessorIndx = AttribIter->second;

        if (AttribIter->first.compare("TANGENT") == 0)
            AttribInfo[ATTRIB_TANGENT].AccessorIndx = AttribIter->second;

        if (AttribIter->first.compare("TEXCOORD_0") == 0)
            AttribInfo[ATTRIB_TEXCOORD_0].AccessorIndx = AttribIter->second;

        if (AttribIter->first.compare("TEXCOORD_1") == 0)
            AttribInfo[ATTRIB_TEXCOORD_1].AccessorIndx = AttribIter->second;

        if (AttribIter->first.compare("COLOR_0") == 0)
            AttribInfo[ATTRIB_COLOR_0].AccessorIndx = AttribIter->second;

        if (AttribIter->first.compare("JOINTS_0") == 0)
            AttribInfo[ATTRIB_JOINTS_0].AccessorIndx = AttribIter->second;

        if (AttribIter->first.compare("WEIGHTS_0") == 0)
            AttribInfo[ATTRIB_WEIGHTS_0].AccessorIndx = AttribIter->second;
    }

    // Need to have at least Indices and position
    if (AttribInfo[ATTRIB_INDICES].AccessorIndx < 0)
    {
        printf("\nError loading %s: Mesh has no indices", filename.c_str());
        return false;
    }
    if (AttribInfo[ATTRIB_POSITION].AccessorIndx < 0)
    {
        printf("\nError loading %s: Mesh has no position data", filename.c_str());
        return false;
    }


    // We now know the  ModelData.accessors[] index for all our data.
    for (uint32_t WhichAttrib = 0; WhichAttrib < NUM_GLTF_ATTRIBS; WhichAttrib++)
    {
        if (AttribInfo[WhichAttrib].AccessorIndx >= 0)
        {
            const tinygltf::Accessor& AccessorData = ModelData.accessors[AttribInfo[WhichAttrib].AccessorIndx];
            AttribInfo[WhichAttrib].BytesPerElem = AccessorData.ByteStride(ModelData.bufferViews[AccessorData.bufferView]);
        }
    }

    if (AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem != 0 && AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem != 8)
    {
        // Do we handle texture coordinates that are not UV?
        printf("\nError loading %s: Texture coordinates are not UV only", filename.c_str());
        return false;
    }


    // Look at the buffer views
    uint32_t NumBufferViews = (uint32_t)ModelData.bufferViews.size();
    for (uint32_t WhichView = 0; WhichView < NumBufferViews; WhichView++)
    {
        const tinygltf::BufferView& ViewData = ModelData.bufferViews[WhichView];
        const char* pTargetString = "Unknown";
        if (ViewData.target == 0x8892)          // GL_ARRAY_BUFFER
            pTargetString = "Array Buffer";
        else if (ViewData.target == 0x8893)     // GL_ELEMENT_ARRAY_BUFFER
            pTargetString = "Index Buffer";

        const tinygltf::Buffer& BufferData = ModelData.buffers[ViewData.buffer];
        // printf("\nBuffer View %d (%s): Bytes = %d; Offset = %d", WhichView, pTargetString, (uint32_t)ViewData.byteLength, (uint32_t)ViewData.byteOffset);
    
        for (uint32_t WhichAttrib = 0; WhichAttrib < NUM_GLTF_ATTRIBS; WhichAttrib++)
        {
            if (WhichView == AttribInfo[WhichAttrib].AccessorIndx)
            {
                AttribInfo[WhichAttrib].BytesTotal = (uint32_t)ViewData.byteLength;
                AttribInfo[WhichAttrib].pData = (void*)(&BufferData.data.at(0) + ViewData.byteOffset);
            }
        }

    }

    // Paranoia Check that same number of positions, normals, and texture coordinates
    uint32_t NumIndices = AttribInfo[ATTRIB_INDICES].BytesTotal / AttribInfo[ATTRIB_INDICES].BytesPerElem;
    uint32_t NumPositions = AttribInfo[ATTRIB_POSITION].BytesTotal / AttribInfo[ATTRIB_POSITION].BytesPerElem;
    uint32_t NumNormals = 0;
    uint32_t NumTexCoords = 0;
    if (AttribInfo[ATTRIB_NORMAL].BytesPerElem != 0)
    {
        NumNormals = AttribInfo[ATTRIB_NORMAL].BytesTotal / AttribInfo[ATTRIB_NORMAL].BytesPerElem;
        if (NumNormals != NumPositions)
        {
            printf("\nError loading %s: Mesh has different number of positions and normals", filename.c_str());
            return false;
        }
    }

    if (AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem != 0)
    {
        NumTexCoords = AttribInfo[ATTRIB_TEXCOORD_0].BytesTotal / AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem;
        if (NumTexCoords != NumPositions)
        {
            printf("\nError loading %s: Mesh has different number of positions and texture coordinates", filename.c_str());
            return false;
        }
    }

    // Finally, we can fill in the actual mesh data
    LOGI("    Mesh Object:");
    LOGI("      %d Indices (%d triangles)", NumIndices, NumIndices / 3);
    LOGI("      %d Positions", NumPositions);
    LOGI("      %d Normals", NumNormals);
    LOGI("      %d UVs", NumTexCoords);

    std::vector<vertex_layout> objVertices;
    for (uint32_t WhichIndx = 0; WhichIndx < NumIndices; WhichIndx++)
    {
        // What is the uint32_t version of the index that may be uint16_t
        uint32_t OneIndx = WhichIndx;
        if (AttribInfo[ATTRIB_INDICES].BytesPerElem == 2)
        {
            uint16_t* pShortPointer = (uint16_t*)AttribInfo[ATTRIB_INDICES].pData;
            uint16_t OneShort = pShortPointer[WhichIndx];
            OneIndx = (uint32_t)OneShort;
        }
        else if(AttribInfo[ATTRIB_INDICES].BytesPerElem == 4)
        {
            uint32_t* pLongPointer = (uint32_t*)AttribInfo[ATTRIB_INDICES].pData;
            uint32_t OneLong = pLongPointer[WhichIndx];
            OneIndx = (uint32_t)OneLong;
        }

        vertex_layout vertex;
        memset(&vertex, 0, sizeof(vertex));

        if (AttribInfo[ATTRIB_POSITION].pData != nullptr)
        {
            float* pPosition = (float*)AttribInfo[ATTRIB_POSITION].pData;
            vertex.pos[0] = pPosition[3 * OneIndx + 0];
            vertex.pos[1] = pPosition[3 * OneIndx + 1];
            vertex.pos[2] = pPosition[3 * OneIndx + 2];
        }

        if (AttribInfo[ATTRIB_NORMAL].pData != nullptr)
        {
            float* pNormals = (float*)AttribInfo[ATTRIB_NORMAL].pData;
            vertex.normal[0] = pNormals[3 * OneIndx + 0];
            vertex.normal[1] = pNormals[3 * OneIndx + 1];
            vertex.normal[2] = pNormals[3 * OneIndx + 2];
        }

        if (AttribInfo[ATTRIB_TANGENT].pData != nullptr)
        {
            float* pTangents = (float*)AttribInfo[ATTRIB_TANGENT].pData;
            vertex.tangent[0] = pTangents[3 * OneIndx + 0];
            vertex.tangent[1] = pTangents[3 * OneIndx + 1];
            vertex.tangent[2] = pTangents[3 * OneIndx + 2];
        }

        if (AttribInfo[ATTRIB_TEXCOORD_0].pData != nullptr)
        {
            float* pTexCoords = (float*)AttribInfo[ATTRIB_TEXCOORD_0].pData;
            vertex.uv[0] = pTexCoords[2 * OneIndx + 0];
            vertex.uv[1] = pTexCoords[2 * OneIndx + 1];
        }

        // Default vertice color is white (debug with pink if needed)
        if (AttribInfo[ATTRIB_COLOR_0].pData != nullptr)
        {
            float *pColor = (float*)AttribInfo[ATTRIB_COLOR_0].pData;
            vertex.color[0] = pColor[4 * OneIndx + 0];
            vertex.color[1] = pColor[4 * OneIndx + 1];
            vertex.color[2] = pColor[4 * OneIndx + 2];
            vertex.color[3] = pColor[4 * OneIndx + 3];
        }
        else
        {
            vertex.color[0] = 1.0f;
            vertex.color[1] = 1.0f;
            vertex.color[2] = 1.0f;
            vertex.color[3] = 1.0f;
        }

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
        vertex_layout vertex;
        memset(&vertex, 0, sizeof(vertex));

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
    assert(pVertexFormat.size() > 0);
    meshObjectOut->Destroy();

    MemoryManager& memoryManager = pVulkan->GetMemoryManager();

    const size_t numVertices = meshObject.m_VertexBuffer.size();

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
            const std::vector<uint32_t> formattedVertexData = CopyFatToFormattedBuffer(meshObject.m_VertexBuffer, pVertexFormat[vertexBufferIdx]);

            meshObjectOut->m_NumVertices = (uint32_t)numVertices;
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

std::vector<uint32_t> MeshObject::CopyFatToFormattedBuffer(const tcb::span<const MeshObjectIntermediate::FatVertex>& fatVertexBuffer, const VertexFormat& vertexFormat)
{
    //
    // Determine the arrangement of the output data (based on the vertexFormat)
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
        if (elementId == "Position")
        {
            positionIndex = i;
        }
        else if (elementId == "UV")
        {
            uv0Index = i;
        }
        else if (elementId == "Color")
        {
            colorIndex = i;
        }
        else if (elementId == "Normal")
        {
            normalIndex = i;
        }
        else if (elementId == "Tangent")
        {
            tangentIndex = i;
        }
        else if (elementId == "Bitangent")
        {
            bitangentIndex = i;
        }
        else
        {
            LOGE("Cannot map vertex elementId %s to the mesh data", elementId.c_str());
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

    return std::move(outputData);
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
                shapeMaterials[faceMaterialId] = &meshObjects.emplace_back();
                shapeMaterials[faceMaterialId]->m_VertexBuffer.reserve(materialFaceCounts[faceMaterialId] * 3/*TODO: could be better!*/);
                shapeMaterials[faceMaterialId]->m_Materials.emplace_back(MaterialDef{ 
                    materials[faceMaterialId].diffuse_texname, 
                    materials[faceMaterialId].bump_texname,
                    materials[faceMaterialId].emissive_texname,
                    materials[faceMaterialId].specular_texname,
                    !materials[faceMaterialId].alpha_texname.empty(),
                    materials[faceMaterialId].illum==7
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

bool StubGltfLoadImageDataFunction(tinygltf::Image*, const int, std::string*,
    std::string*, int, int,
    const unsigned char*, int, void*)
{
    return true;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<MeshObjectIntermediate> MeshObjectIntermediate::LoadGLTF(AssetManager& assetManager, const std::string& filename)
{
    std::vector<MeshObjectIntermediate> meshObjects;

    tinygltf::Model ModelData;

    std::string err;
    std::string warn;

    gltfAttribInfo  AttribInfo[NUM_GLTF_ATTRIBS];
    for (uint32_t WhichAttrib = 0; WhichAttrib < NUM_GLTF_ATTRIBS; WhichAttrib++)
    {
        AttribInfo[WhichAttrib].AccessorIndx = -1;
        AttribInfo[WhichAttrib].BytesPerElem = 0;
        AttribInfo[WhichAttrib].BytesTotal = 0;
        AttribInfo[WhichAttrib].pData = nullptr;
    }

    {
        tinygltf::TinyGLTF ModelLoader;

        // Set the image loader (stubbed out)
        ModelLoader.SetImageLoader(&StubGltfLoadImageDataFunction, nullptr);

        ModelLoader.SetFsCallbacks( tinygltf::FsCallbacks{ &Gltf_FileExists, &Gltf_ExpandFilePath, &Gltf_ReadWholeFile, &Gltf_WriteWholeFile, &assetManager } );

        bool RetVal = ModelLoader.LoadASCIIFromFile(&ModelData, &err, &warn, filename);
        if (!warn.empty())
        {
            printf("\nWarning loading %s: %s", filename.c_str(), warn.c_str());
        }

        if (!err.empty())
        {
            printf("\nError loading %s: %s", filename.c_str(), err.c_str());
        }

        if (!RetVal)
        {
            // Assume the error was logged above
            return {};
        }
    }

    // Start with the "Scene"...
    if (ModelData.scenes.size() == 0)
    {
        printf("\nError loading %s: File did not contain a scene node", filename.c_str());
        return {};
    }
    if (ModelData.defaultScene >= ModelData.scenes.size())
    {
        printf("\nError loading %s: Default scene node is out of range", filename.c_str());
        return {};
    }

    const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];

    // ... find the node used by this scene ...
    uint32_t NumNodes = (uint32_t)SceneData.nodes.size();
    if (NumNodes == 0)
    {
        printf("\nError loading %s: Default scene does not contain any nodes", filename.c_str());
        return {};
    }
    if (SceneData.nodes[0] >= ModelData.nodes.size())
    {
        printf("\nError loading %s: Default scene node is invalid index", filename.c_str());
        return {};
    }

    LOGI("Mesh gltf %s has %d Node[s]", filename.c_str(), (int)ModelData.nodes.size());

    // Get a count of the total number of mesh object we wll be outputting (so we can pre-size the array).
    size_t totalPrimitives = 0;
    const auto CountPrimitives = [&filename](const tinygltf::Model& ModelData, const std::vector<int>& SceneNodeIndices) {
        // Define a lambda inside the lambda so we can call recursively!
        auto count_lambda = [&filename](const tinygltf::Model& ModelData, const std::vector<int>& SceneNodeIndices, auto& count_lambda_ref) {
            size_t primitives = 0;
            for (int sceneNodeIdx : SceneNodeIndices) {
                const tinygltf::Node& NodeData = ModelData.nodes[sceneNodeIdx];
                if (NodeData.mesh >= 0)
                {
                    if (NodeData.mesh >= ModelData.meshes.size())
                    {
                        printf("\nError loading %s: Node mesh is invalid index", filename.c_str());
                        return (size_t)0;
                    }
                    const tinygltf::Mesh& MeshData = ModelData.meshes[NodeData.mesh];
                    primitives += MeshData.primitives.size();
                }
                if (!NodeData.children.empty())
                {
                    primitives += count_lambda_ref(ModelData, NodeData.children, count_lambda_ref);
                }
            }
            return primitives;
        };
        return count_lambda(ModelData, SceneNodeIndices, count_lambda);
    };

    totalPrimitives = CountPrimitives(ModelData, SceneData.nodes);
    meshObjects.reserve(totalPrimitives);

    // Go through all the scene nodes, outputting models for each
    const auto OutputModels = [&filename, &AttribInfo, &meshObjects](const tinygltf::Model& ModelData, const std::vector<int>& SceneNodeIndices) {
        // Define a lambda inside the lambda so we can call recursively!
        auto output_lambda = [&filename, &AttribInfo, &meshObjects](const tinygltf::Model& ModelData, const std::vector<int>& SceneNodeIndices, auto& output_lambda_ref) {
            for (int sceneNodeIdx : SceneNodeIndices) {
                const tinygltf::Node& NodeData = ModelData.nodes[sceneNodeIdx];
                if (NodeData.mesh >= 0)
                {
                    if (NodeData.mesh >= ModelData.meshes.size())
                    {
                        printf("\nError loading %s: Node mesh is invalid index", filename.c_str());
                        return false;
                    }
                    // ... get the mesh for this node ...
                    const tinygltf::Mesh& MeshData = ModelData.meshes[NodeData.mesh];

                    for (uint32_t WhichPrimitive = 0; WhichPrimitive < MeshData.primitives.size(); ++WhichPrimitive)
                    {
                        const tinygltf::Primitive& PrimitiveData = MeshData.primitives[WhichPrimitive];
                        if (PrimitiveData.mode != TINYGLTF_MODE_TRIANGLES)
                        {
                            // we dont handle anything other than triangles currently.
                            continue;
                        }

                        uint32_t NumAttribs = (uint32_t)PrimitiveData.attributes.size();

                        // Indices are not "parsed" but can be accessed directly
                        AttribInfo[ATTRIB_INDICES].AccessorIndx = PrimitiveData.indices;

                        std::map<std::string, int>::const_iterator AttribIter;
                        for (AttribIter = PrimitiveData.attributes.begin(); AttribIter != PrimitiveData.attributes.end(); AttribIter++)
                        {
                            if (AttribIter->first.compare("POSITION") == 0)
                                AttribInfo[ATTRIB_POSITION].AccessorIndx = AttribIter->second;

                            if (AttribIter->first.compare("NORMAL") == 0)
                                AttribInfo[ATTRIB_NORMAL].AccessorIndx = AttribIter->second;

                            if (AttribIter->first.compare("TANGENT") == 0)
                                AttribInfo[ATTRIB_TANGENT].AccessorIndx = AttribIter->second;

                            if (AttribIter->first.compare("TEXCOORD_0") == 0)
                                AttribInfo[ATTRIB_TEXCOORD_0].AccessorIndx = AttribIter->second;

                            if (AttribIter->first.compare("TEXCOORD_1") == 0)
                                AttribInfo[ATTRIB_TEXCOORD_1].AccessorIndx = AttribIter->second;

                            if (AttribIter->first.compare("COLOR_0") == 0)
                                AttribInfo[ATTRIB_COLOR_0].AccessorIndx = AttribIter->second;

                            if (AttribIter->first.compare("JOINTS_0") == 0)
                                AttribInfo[ATTRIB_JOINTS_0].AccessorIndx = AttribIter->second;

                            if (AttribIter->first.compare("WEIGHTS_0") == 0)
                                AttribInfo[ATTRIB_WEIGHTS_0].AccessorIndx = AttribIter->second;
                        }

                        // Need to have at least Indices and position
                        if (AttribInfo[ATTRIB_INDICES].AccessorIndx < 0)
                        {
                            printf("\nError loading %s: Mesh has no indices", filename.c_str());
                            return false;
                        }
                        if (AttribInfo[ATTRIB_POSITION].AccessorIndx < 0)
                        {
                            printf("\nError loading %s: Mesh has no position data", filename.c_str());
                            return false;
                        }


                        // We now know the  ModelData.accessors[] index for all our data.
                        for (uint32_t WhichAttrib = 0; WhichAttrib < NUM_GLTF_ATTRIBS; WhichAttrib++)
                        {
                            if (AttribInfo[WhichAttrib].AccessorIndx >= 0)
                            {
                                const tinygltf::Accessor& AccessorData = ModelData.accessors[AttribInfo[WhichAttrib].AccessorIndx];
                                AttribInfo[WhichAttrib].BytesPerElem = AccessorData.ByteStride(ModelData.bufferViews[AccessorData.bufferView]);
                            }
                        }

                        if (AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem != 0 && AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem != 8)
                        {
                            // Do we handle texture coordinates that are not UV?
                            printf("\nError loading %s: Texture coordinates are not UV only", filename.c_str());
                            return false;
                        }

                        // Look at the buffer views
                        uint32_t NumBufferViews = (uint32_t)ModelData.bufferViews.size();
                        for (uint32_t WhichView = 0; WhichView < NumBufferViews; WhichView++)
                        {
                            const tinygltf::BufferView& ViewData = ModelData.bufferViews[WhichView];
                            const char* pTargetString = "Unknown";
                            if (ViewData.target == 0x8892)          // GL_ARRAY_BUFFER
                                pTargetString = "Array Buffer";
                            else if (ViewData.target == 0x8893)     // GL_ELEMENT_ARRAY_BUFFER
                                pTargetString = "Index Buffer";

                            const tinygltf::Buffer& BufferData = ModelData.buffers[ViewData.buffer];
                            // printf("\nBuffer View %d (%s): Bytes = %d; Offset = %d", WhichView, pTargetString, (uint32_t)ViewData.byteLength, (uint32_t)ViewData.byteOffset);

                            for (uint32_t WhichAttrib = 0; WhichAttrib < NUM_GLTF_ATTRIBS; WhichAttrib++)
                            {
                                if (WhichView == AttribInfo[WhichAttrib].AccessorIndx)
                                {
                                    AttribInfo[WhichAttrib].BytesTotal = (uint32_t)ViewData.byteLength;
                                    AttribInfo[WhichAttrib].pData = (void*)(&BufferData.data.at(0) + ViewData.byteOffset);
                                }
                            }
                        }

                        // Paranoia Check that same number of positions, normals, and texture coordinates
                        uint32_t NumIndices = AttribInfo[ATTRIB_INDICES].BytesTotal / AttribInfo[ATTRIB_INDICES].BytesPerElem;
                        uint32_t NumPositions = AttribInfo[ATTRIB_POSITION].BytesTotal / AttribInfo[ATTRIB_POSITION].BytesPerElem;
                        uint32_t NumNormals = 0;
                        uint32_t NumTexCoords = 0;
                        if (AttribInfo[ATTRIB_NORMAL].BytesPerElem != 0)
                        {
                            NumNormals = AttribInfo[ATTRIB_NORMAL].BytesTotal / AttribInfo[ATTRIB_NORMAL].BytesPerElem;
                            if (NumNormals != NumPositions)
                            {
                                printf("\nError loading %s: Mesh has different number of positions and normals", filename.c_str());
                                return false;
                            }
                        }

                        if (AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem != 0)
                        {
                            NumTexCoords = AttribInfo[ATTRIB_TEXCOORD_0].BytesTotal / AttribInfo[ATTRIB_TEXCOORD_0].BytesPerElem;
                            if (NumTexCoords != NumPositions)
                            {
                                printf("\nError loading %s: Mesh has different number of positions and texture coordinates", filename.c_str());
                                return false;
                            }
                        }

                        // Finally, we can fill in the actual mesh data
                        LOGI("    Mesh Object:");
                        LOGI("      %d Indices (%d triangles)", NumIndices, NumIndices / 3);
                        LOGI("      %d Positions", NumPositions);
                        LOGI("      %d Normals", NumNormals);
                        LOGI("      %d UVs", NumTexCoords);

                        const int materialIdx = PrimitiveData.material;

                        auto& meshObject = meshObjects.emplace_back();

                        if (PrimitiveData.material >= 0)/*-1 is valid*/
                        {
                            // Pull out the relevant material information.
                            const auto& material = ModelData.materials[PrimitiveData.material];

                            int baseColorTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                            baseColorTextureIndex = baseColorTextureIndex >= 0 ? ModelData.textures[baseColorTextureIndex].source : -1;

                            int normalIndex = material.normalTexture.index;
                            normalIndex = normalIndex >= 0 ? ModelData.textures[normalIndex].source : -1;

                            int emissiveIndex = material.emissiveTexture.index;
                            emissiveIndex = emissiveIndex >= 0 ? ModelData.textures[emissiveIndex].source : -1;

                            int pbrIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
                            pbrIndex = pbrIndex >= 0 ? ModelData.textures[pbrIndex].source : -1;

                            meshObject.m_Materials.emplace_back(MeshObjectIntermediate::MaterialDef{
                                baseColorTextureIndex >= 0 ? ModelData.images[baseColorTextureIndex].uri : "",
                                normalIndex >= 0 ? ModelData.images[normalIndex].uri : "",
                                emissiveIndex >= 0 ? ModelData.images[emissiveIndex].uri : "",
                                pbrIndex >= 0 ? ModelData.images[pbrIndex].uri : "",
                                material.alphaMode == "MASK",
                                material.alphaMode == "BLEND" });
                        }
                        meshObject.m_VertexBuffer.reserve(NumPositions);

                        // Copy over the index buffer data.
                        if (AttribInfo[ATTRIB_INDICES].BytesPerElem == 4)
                        {
                            uint32_t* p32 = (uint32_t*)AttribInfo[ATTRIB_INDICES].pData;
                            tcb::span<uint32_t> indicesSpan{ p32, NumIndices };
                            meshObject.m_IndexBuffer.emplace< std::vector<uint32_t>>(indicesSpan.begin(), indicesSpan.end());
                        }
                        else if (AttribInfo[ATTRIB_INDICES].BytesPerElem == 2)
                        {
                            uint16_t* p16 = (uint16_t*)AttribInfo[ATTRIB_INDICES].pData;
                            tcb::span<uint16_t> indicesSpan{ p16, NumIndices };
                            meshObject.m_IndexBuffer.emplace< std::vector<uint16_t>>( indicesSpan.begin(), indicesSpan.end() );
                        }
                        else
                        {
                            printf("\nError loading %s: Mesh has invalid BytesPerElem for indices", filename.c_str());
                            return false;
                        }

                        float* pPosition = (float*)AttribInfo[ATTRIB_POSITION].pData;
                        float* pNormals = (float*)AttribInfo[ATTRIB_NORMAL].pData;
                        float* pTangents = (float*)AttribInfo[ATTRIB_TANGENT].pData;
                        float* pTexCoords = (float*)AttribInfo[ATTRIB_TEXCOORD_0].pData;
                        float* pColor = (float*)AttribInfo[ATTRIB_COLOR_0].pData;

                        for (uint32_t WhichVert = 0; WhichVert < NumPositions; ++WhichVert)
                        {
                            FatVertex vertex;
                            if (pPosition != nullptr)
                            {
                                vertex.position[0] = *pPosition++;
                                vertex.position[1] = *pPosition++;
                                vertex.position[2] = *pPosition++;
                            }

                            if (pNormals != nullptr)
                            {
                                vertex.normal[0] = *pNormals++;
                                vertex.normal[1] = *pNormals++;
                                vertex.normal[2] = *pNormals++;
                            }

                            if (pTangents != nullptr)
                            {
                                vertex.tangent[0] = *pTangents++;
                                vertex.tangent[1] = *pTangents++;
                                vertex.tangent[2] = *pTangents++;
                            }

                            if (pTexCoords != nullptr)
                            {
                                vertex.uv0[0] = *pTexCoords++;
                                vertex.uv0[1] = *pTexCoords++;
                            }

                            // Default vertice color is white (debug with pink if needed)
                            if (pColor != nullptr)
                            {
                                vertex.color[0] = *pColor++;
                                vertex.color[1] = *pColor++;
                                vertex.color[2] = *pColor++;
                                vertex.color[3] = *pColor++;
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
                // Recurse in to children
                if (!NodeData.children.empty())
                {
                    if (!output_lambda_ref(ModelData, NodeData.children, output_lambda_ref))
                    {
                        return false;   // Error
                    }
                }
            }
            return true;
        };
        return output_lambda(ModelData, SceneNodeIndices, output_lambda);
    };

    if (!OutputModels(ModelData, SceneData.nodes))
    {
        return {};
    }

    // Gather some statistics
    size_t totalVerts = 0;
    for (const auto& mesh : meshObjects)
    {
        totalVerts += mesh.m_VertexBuffer.size();
    }
    LOGI("Model total Vertices: %zu", totalVerts);

    return meshObjects;
}


///////////////////////////////////////////////////////////////////////////////
    
bool MeshObject::Destroy()
{
    m_NumVertices = 0;
    m_VertexBuffers.clear();
    m_IndexBuffer.reset();

    return true;
}
