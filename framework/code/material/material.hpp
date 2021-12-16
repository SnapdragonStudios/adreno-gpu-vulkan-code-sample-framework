// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include "descriptorSetLayout.hpp"

// Forward declarations
class Vulkan;
class Shader;
class ShaderPass;
class VulkanTexInfo;

/// @defgroup Material
/// Material and Shader loader.
/// Handles creation of descriptor sets, buffer binding, shader binaries (everything in Vulkan that describes and is used to render a shader).
/// 
/// Typically user (application) writes a Json description for each 'Shader' that describes the inputs, outputs, and internal state of the shader, and the shader code (glsl).
/// The user then uses ShaderManager::AddShader to register (and load) each Json file and the shader binaries.
/// 
/// From there the user uses MaterialManager::CreateMaterial to create a Material instance of the Shader (a Material contains bindings to the various texture/buffer inputs that a Shader requires - there can be many Materials using the same Shader (each Material with different textures, vertex buffers and/or uniform buffers etc)
/// 
/// The Material returned by CreateMaterial can be used to Create a Drawable or Computable object that wraps everything together with one convenient interface!
/// 
/// For more complex models the user should use DrawableLoader::LoadDrawable to load the mesh model file (and return a vector of Drawables).  This api greatly simplifies the material creation and binding, splitting model meshes across material boundaries, automatically detecting instances (optionally).
/// 


/// An instance of a ShaderPass material.
/// @ingroup Material
class MaterialPass
{
    MaterialPass(const MaterialPass&) = delete;
    MaterialPass& operator=(const MaterialPass&) = delete;
public:
    MaterialPass(MaterialPass&&) noexcept;
    struct ImageInfo {
        ImageInfo(ImageInfo&&) = default;
        ImageInfo(const ImageInfo&) = delete;
        ImageInfo& operator=(const ImageInfo&) = delete;
        ImageInfo(const VulkanTexInfo&);
        VkImage image;
        VkImageView imageView;
        VkImageLayout imageLayout;
    };

    typedef std::vector<const VulkanTexInfo*> tPerFrameTexInfo;
    typedef std::vector<VkBuffer> tPerFrameVkBuffer;
    typedef std::vector <std::pair<tPerFrameTexInfo, DescriptorSetLayout::BindingTypeAndIndex>> tTextureBindings;
    typedef std::vector <std::pair<ImageInfo, DescriptorSetLayout::BindingTypeAndIndex>> tImageBindings;
    typedef std::vector <std::pair<tPerFrameVkBuffer, DescriptorSetLayout::BindingTypeAndIndex>> tBufferBindings;

    MaterialPass(Vulkan& vulkan, const ShaderPass&, VkDescriptorPool&&, std::vector<VkDescriptorSet>&&, tTextureBindings&&, tImageBindings&&, tBufferBindings&&);
    ~MaterialPass();

    /// Get the descriptor set for the (numbered) frame buffer index, allows for a single descriptor set identical for all frames if required.
    const auto& GetVkDescriptorSet(uint32_t bufferIndex) const { return mDescriptorSets[mDescriptorSets.size() > 1 ? bufferIndex : 0]; }
    const auto& GetVkDescriptorSets() const { return mDescriptorSets; }

    const auto& GetTextureBindings() const  { return mTextureBindings; }
    const auto& GetImageBindings() const    { return mImageBindings; }

    bool UpdateDescriptorSets(uint32_t bufferIdx);

    const ShaderPass& mShaderPass;
protected:
    Vulkan& mVulkan;

    // Vulkan objects
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;    ///< array of descriptor sets (one per NUM_VULKAN_BUFFERS)

    tTextureBindings mTextureBindings;              ///< Images (textures) (with sampler) considered readonly
    tImageBindings mImageBindings;                  ///< Images that may be bound as writable (or read/write).
    tBufferBindings mBufferBindings;
};


/// An instance of a Shader material.
/// Container for MaterialPasses and reference to this material's Shader
/// @ingroup Material
class Material
{
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
public:
    Material(const Shader& shader);
    Material(Material&&) noexcept;
    ~Material();

    void AddMaterialPass(const std::string& passName, MaterialPass&& pass)
    {
        if (m_materialPassNamesToIndex.try_emplace(passName, (uint32_t)m_materialPasses.size()).second == true)
        {
            m_materialPasses.emplace_back(std::move(pass));
        }
        // else pass name already exists - do nothing!
    }

    const MaterialPass* GetMaterialPass(const std::string& passName) const;
    const auto& GetMaterialPasses() const { return m_materialPasses; }

    bool UpdateDescriptorSets(uint32_t bufferIdx);

    const Shader& m_shader;
protected:
    std::map<std::string, uint32_t> m_materialPassNamesToIndex;   // pass name to index in m_materialPasses
    std::vector<MaterialPass> m_materialPasses;
};
