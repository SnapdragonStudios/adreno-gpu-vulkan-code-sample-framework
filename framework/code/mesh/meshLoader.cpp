//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "meshLoader.hpp"
#include "system/assetManager.hpp"
#include <string>
#include <glm/gtx/quaternion.hpp>


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

bool StubGltfLoadImageDataFunction(tinygltf::Image*, const int, std::string*,
    std::string*, int, int,
    const unsigned char*, int, void*)
{
    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool MeshLoader::LoadGlftModel(AssetManager& assetManager, const std::string& filename, tinygltf::Model& ModelData)
{
    std::string err;
    std::string warn;

    LOGI("Loading GLTF: %s...", filename.c_str());

    // Load the glft model.
    {
        tinygltf::TinyGLTF ModelLoader;

        // Set the image loader (stubbed out)
        ModelLoader.SetImageLoader(&StubGltfLoadImageDataFunction, nullptr);

        ModelLoader.SetFsCallbacks(tinygltf::FsCallbacks{ &Gltf_FileExists, &Gltf_ExpandFilePath, &Gltf_ReadWholeFile, &Gltf_WriteWholeFile, &assetManager });

        bool RetVal = ModelLoader.LoadASCIIFromFile(&ModelData, &err, &warn, filename);
        if (!warn.empty())
        {
            LOGE("\nWarning loading %s: %s", filename.c_str(), warn.c_str());
        }

        if (!err.empty())
        {
            LOGE("\nError loading %s: %s", filename.c_str(), err.c_str());
        }

        if (!RetVal)
        {
            // Assume the error was logged above
            return false;
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

static glm::mat4 Mat4FromNode(const tinygltf::Node& Node)
{
    if (Node.matrix.size() == 16)
    {
        const auto* p = &Node.matrix[0];
        return glm::mat4(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }
    else
    {
        glm::mat4 Matrix = glm::identity<glm::mat4>();
        if (Node.translation.size() == 3)
        {
            const auto* p = &Node.translation[0];
            Matrix = glm::translate(glm::vec3(p[0], p[1], p[2]));
        }
        if (Node.rotation.size() == 4)
        {
            const auto* p = &Node.rotation[0];
            glm::mat4 rot44 = glm::toMat4(glm::f64quat(/*xyzw in gltf*/ p[3], p[0], p[1], p[2]));
            Matrix = Matrix * rot44;
        }
        if (Node.scale.size() == 3)
        {
            const auto* p = &Node.scale[0];
            Matrix = Matrix * glm::scale(glm::vec3(p[0], p[1], p[2]));
        }
        return Matrix;
    }
}

///////////////////////////////////////////////////////////////////////////////

MeshLoader::NodeTransform::NodeTransform(const tinygltf::Node& Node) : Matrix( Mat4FromNode(Node) )
{
}

///////////////////////////////////////////////////////////////////////////////

MeshLoader::NodeTransform::NodeTransform(const NodeTransform& Parent, const tinygltf::Node& Node)
    : Matrix( Parent.Matrix * Mat4FromNode(Node) )
{
}

///////////////////////////////////////////////////////////////////////////////

bool MeshLoaderModelSceneSanityCheck::operator()(const tinygltf::Model& ModelData)
{
    if (ModelData.scenes.size() == 0)
    {
        printf("\nError loading %s: File did not contain a scene node", m_filename.c_str());
        return false;
    }
    if (ModelData.defaultScene >= ModelData.scenes.size())
    {
        printf("\nError loading %s: Default scene node is out of range", m_filename.c_str());
        return false;
    }

    const tinygltf::Scene& SceneData = ModelData.scenes[ModelData.defaultScene];

    // ... find the head node used by this scene ...
    uint32_t NumNodes = (uint32_t)SceneData.nodes.size();
    if (NumNodes == 0)
    {
        printf("\nError loading %s: Default scene does not contain any nodes", m_filename.c_str());
        return false;
    }
    if (SceneData.nodes[0] >= ModelData.nodes.size())
    {
        printf("\nError loading %s: Default scene node is invalid index", m_filename.c_str());
        return false;
    }

    LOGI("Mesh gltf %s has %d SkeletonNodeData[s]", m_filename.c_str(), (int)ModelData.nodes.size());

    return true;
}
