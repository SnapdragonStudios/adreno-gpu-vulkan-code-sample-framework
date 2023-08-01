//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#ifdef OS_WINDOWS
#define VK_ENABLE_BETA_EXTENSIONS
#endif
#include <vulkan/vulkan.h>
#include "descriptorSetLayout.hpp"
#include "vulkan/pipelineLayout.hpp"
#include "vulkan/shader.hpp"
#include "vulkan/specializationConstantsLayout.hpp"

// Forward declarations
class Vulkan;
template<typename T_GFXAPI> class TextureT;
class Texture;
class VertexElementData;

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

/// Reference to a VkImage.
/// Does not have ownership over referenced VkImage or VkImageView and lifetime of those object should be longer than the referencing @ImageInfo (no reference counting).
/// @ingroup Material
struct ImageInfo {
    ImageInfo(ImageInfo&&) noexcept;
    ImageInfo& operator=(ImageInfo&&) noexcept;
    ImageInfo& operator=(const ImageInfo&) = delete;
    ImageInfo(const ImageInfo&);
    ImageInfo(const TextureT<Vulkan>&);
    ImageInfo(const Texture&);
    VkImage image;
    VkImageView imageView;
    uint32_t imageViewNumMips;
    uint32_t imageViewFirstMip;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL;
};

/// Reference to VkBuffer (with an offset).
/// Does not have ownership of the buffer (and so lifetime of the buffer should be longer than the referencing @VkBufferAndOffset)
struct VkBufferAndOffset {
    VkBuffer buffer;
    uint32_t offset = 0;
    constexpr bool operator==( const VkBufferAndOffset& other ) const = default;
};

struct tPerFrameVkBuffer
{
    typedef std::vector<VkBufferAndOffset> tBuffers;
    tBuffers buffers;

    template< typename InputIt> requires ( std::is_same_v<typename std::iterator_traits<InputIt>::value_type, VkBuffer > )
    tPerFrameVkBuffer( InputIt first, InputIt last )
    {
        while (first != last)
            buffers.push_back( { *first++, 0 } );
    }
    template< typename InputIt > requires (std::is_same_v<typename tBuffers::iterator, InputIt>)
    tPerFrameVkBuffer( InputIt first, InputIt last ) : buffers( first, last )
    {
    }

    template<typename InputContainer, typename ValueType = std::decay_t<decltype(*std::begin( std::declval<InputContainer>() ))>>
    tPerFrameVkBuffer( const InputContainer& container )
    {
        for (auto item : container)
            buffers.push_back( { item } );
    }
    tPerFrameVkBuffer( const VkBufferAndOffset& bufferAndOffset ) noexcept
    {
        buffers.push_back( bufferAndOffset );
    }
    tPerFrameVkBuffer( VkBuffer buffer ) noexcept
    {
        buffers.push_back( { buffer, 0 } );
    }
    explicit tPerFrameVkBuffer( tPerFrameVkBuffer&& other ) noexcept : buffers( std::move( other.buffers ) ) {}
    tPerFrameVkBuffer& operator=( tPerFrameVkBuffer&& other ) noexcept {
        if (this != &other) {
            buffers = std::move( other.buffers );
        }
        return *this;
    }
    tPerFrameVkBuffer( const tPerFrameVkBuffer& other ) noexcept : buffers( other.buffers ) {}
    tPerFrameVkBuffer& operator=( const tPerFrameVkBuffer& other ) noexcept {
        if (this != &other) {
            buffers = other.buffers;
        }
        return *this;
    }
    tPerFrameVkBuffer() = default;

    constexpr size_t size() const noexcept { return buffers.size(); }
    constexpr tBuffers::iterator begin() noexcept { return buffers.begin(); }
    constexpr tBuffers::const_iterator begin() const noexcept { return buffers.begin(); }
    constexpr tBuffers::const_iterator cbegin() const noexcept { return buffers.begin(); }
    constexpr tBuffers::iterator end() { return buffers.end(); }
    constexpr tBuffers::const_iterator end() const { return buffers.end(); }
    constexpr tBuffers::const_iterator cend() const { return buffers.cend(); }
    constexpr tBuffers::value_type& operator[]( const tBuffers::size_type x ) noexcept { return buffers[x]; }
    constexpr const tBuffers::value_type& operator[]( const tBuffers::size_type x ) const noexcept { return buffers[x]; }
};

/// Specialization constant data for a MaterialPass.
/// @ingroup Material
class SpecializationConstants
{
    SpecializationConstants( const SpecializationConstants& ) = delete;
    SpecializationConstants operator=( const SpecializationConstants& ) = delete;
public:
    SpecializationConstants( SpecializationConstants&& ) noexcept = default;
    SpecializationConstants() noexcept = default;
    bool Init(const SpecializationConstantsLayout<Vulkan>& layout, const std::span<VertexElementData> constants);

    const VkSpecializationInfo* GetVkSpecializationInfo() const { return mSpecializationData.has_value() ? &mSpecializationData.value().specializationInfo : nullptr; }

private:
    struct VulkanSpecializationData
    {
        VulkanSpecializationData( const VulkanSpecializationData& ) = delete;
        VulkanSpecializationData operator=( const VulkanSpecializationData& ) = delete;

        VulkanSpecializationData() noexcept : specializationInfo() {}
        ~VulkanSpecializationData() { delete[] (std::byte*) specializationInfo.pData/*we own just the data*/; specializationInfo.pData = nullptr; }
        VulkanSpecializationData( VulkanSpecializationData&& ) noexcept;
        VkSpecializationInfo specializationInfo;
    };
    // container for specialization data prepared for vulkan use!
    std::optional<VulkanSpecializationData> mSpecializationData;
};


/// An instance of a ShaderPass material.
/// @ingroup Material
class MaterialPass
{
    MaterialPass(const MaterialPass&) = delete;
    MaterialPass& operator=(const MaterialPass&) = delete;
public:
    MaterialPass(MaterialPass&&) noexcept;
#if VK_KHR_acceleration_structure
    typedef VkAccelerationStructureKHR AccelerationStructureHandle;
#else
    typedef void* AccelerationStructureHandle;  // unused but defined to be void*
#endif

    typedef std::vector<const Texture*> tPerFrameTexInfo;
    typedef std::vector<ImageInfo> tPerFrameImageInfo;

    typedef std::vector<AccelerationStructureHandle> tPerFrameVkAccelerationStructure;
    typedef std::vector <std::pair<tPerFrameTexInfo, DescriptorSetLayout::BindingTypeAndIndex>> tTextureBindings;
    typedef std::vector <std::pair<tPerFrameImageInfo, DescriptorSetLayout::BindingTypeAndIndex>> tImageBindings;
    typedef std::vector <std::pair<tPerFrameVkBuffer, DescriptorSetLayout::BindingTypeAndIndex>> tBufferBindings;
    typedef std::vector <std::pair<tPerFrameVkAccelerationStructure, DescriptorSetLayout::BindingTypeAndIndex>> tAccelerationStructureBindings;

    MaterialPass(Vulkan& vulkan, const ShaderPass<Vulkan>&, VkDescriptorPool&&, std::vector<VkDescriptorSet>&&, std::vector<VkDescriptorSetLayout>&&, tTextureBindings&&, tImageBindings&&, tBufferBindings&&, tAccelerationStructureBindings&&, SpecializationConstants&&);
    ~MaterialPass();

    /// Get the descriptor set for the (numbered) frame buffer index, allows for a single descriptor set identical for all frames if required.
    const auto& GetVkDescriptorSet(uint32_t bufferIndex) const { return mDescriptorSets[mDescriptorSets.size() > 1 ? bufferIndex : 0]; }
    const auto& GetVkDescriptorSets() const { return mDescriptorSets; }
    const auto& GetPipelineLayout() const { return mDynamicPipelineLayout; }
    const auto& GetSpecializationConstants() const { return mSpecializationConstants; };

    const auto& GetTextureBindings() const  { return mTextureBindings; }
    const auto& GetImageBindings() const    { return mImageBindings; }
    const auto& GetBufferBindings() const   { return mBufferBindings; }

    bool UpdateDescriptorSets(uint32_t bufferIdx);
    bool UpdateDescriptorSetBinding(uint32_t bufferIdx, const std::string& bindingName, const TextureT<Vulkan>& newTexture) const;

    const ShaderPass<Vulkan>& mShaderPass;
protected:
    Vulkan& mVulkan;

    // Vulkan objects
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;    ///< array of descriptor sets (usually one or one per NUM_VULKAN_BUFFERS)
    std::vector<VkDescriptorSetLayout> mDynamicDescriptorSetLayouts;///< array of descriptor set layouts specific for to this materialPass (usually they are shared across all materials with a specific shader, except in the case of descriptor sets that are 'dynamically' sized to fit the material specific contents)
    PipelineLayout<Vulkan> mDynamicPipelineLayout; ///< pipeline layout specific to this materiaPass (usually shaderPass contains the pipeline layout but for materials with 'dynamic' descriptor set layouts we have to have a unique pipeline per materialPass
    SpecializationConstants mSpecializationConstants; ///< block of specialization constants for this material pass

    tTextureBindings mTextureBindings;              ///< Images (textures) (with sampler) considered readonly
    tImageBindings mImageBindings;                  ///< Images that may be bound as writable (or read/write).
    tBufferBindings mBufferBindings;
    tAccelerationStructureBindings mAccelerationStructureBindings;
};


/// An instance of a Shader material.
/// Container for MaterialPasses and reference to this material's Shader
/// @ingroup Material
class Material
{
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
public:
    Material(const ShaderT<Vulkan>& shader, uint32_t numFramebuffers);
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
    uint32_t GetNumFrameBuffers() const { return m_numFramebuffers; }

    bool UpdateDescriptorSets(uint32_t bufferIdx);

    /// @brief Update a single value in a descriptor set
    /// Not optimized for being called multiple times per set (per frame).  Intended to be used sparingly.
    /// @param bufferIdx 
    /// @return true on success
    bool UpdateDescriptorSetBinding(uint32_t bufferIdx, const std::string& bindingName, const TextureT<Vulkan>& newTexture) const;

    const ShaderT<Vulkan>& m_shader;
protected:
    std::map<std::string, uint32_t> m_materialPassNamesToIndex; /// pass name to index in m_materialPasses
    std::vector<MaterialPass> m_materialPasses;
    const uint32_t m_numFramebuffers;                           /// more accurately 'number of frames of buffers'.    
};
