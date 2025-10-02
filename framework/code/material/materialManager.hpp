//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <string>
#include <memory>
#include <functional>
//#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

// Forward declarations
class GraphicsApiBase;
class Vulkan;
class Material;
class MaterialPass;
class ShaderDescription;
class Shader;
template<typename T_GFXAPI> class ShaderT;
template<typename T_GXFAPI> class ShaderPass;
class Texture;
struct ImageInfo;
struct tPerFrameVkBuffer;
class VertexElementData;


/// Helper class for creating Material (base class, expected for there to be a graphics api specific derived class)
/// @ingroup Material
class MaterialManager
{
#if VK_KHR_acceleration_structure
    typedef VkAccelerationStructureKHR AccelerationStructureHandle;
#else
    typedef void* AccelerationStructureHandle;
#endif

    MaterialManager(const MaterialManager&) = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;
protected:
    MaterialManager() noexcept {};

public:
    virtual ~MaterialManager() {};
    typedef std::vector<const Texture*> tPerFrameTexInfo;
    typedef std::vector<AccelerationStructureHandle> tPerFrameVkAccelerationStructure;

    virtual Material CreateMaterial(GraphicsApiBase&, const Shader& shader, uint32_t numFrameBuffers,
                                    const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader,
                                    const std::function<const tPerFrameVkBuffer(const std::string&)>& bufferLoader,
                                    const std::function<const ImageInfo(const std::string&)>& imageLoader = nullptr,
                                    const std::function<const tPerFrameVkAccelerationStructure(const std::string&)>& accelerationStructureLoader = nullptr,
                                    const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader = nullptr) const = 0;
};


/// Helper class for creating Material 
/// @ingroup Material
template<typename T_GFXAPI>
class MaterialManagerT : public MaterialManager
{
#if VK_KHR_acceleration_structure
    typedef VkAccelerationStructureKHR AccelerationStructureHandle;
#else
    typedef void* AccelerationStructureHandle;
#endif

    MaterialManagerT<T_GFXAPI>& operator=(const MaterialManagerT<T_GFXAPI>&) = delete;
    MaterialManagerT(const MaterialManagerT<T_GFXAPI>&) = delete;
public:
    typedef std::vector<const Texture*> tPerFrameTexInfo;
    typedef std::vector<AccelerationStructureHandle> tPerFrameVkAccelerationStructure;

    MaterialManagerT();
    ~MaterialManagerT();

    /// Create a material to be used for rendering (potentially contains multiple passes)
    /// If numFrameBuffers is not 1 there will be numFrameBuffers descriptor sets created (so different buffers can be bound on different 'frames', although the layout is fixed)
    Material CreateMaterial(GraphicsApiBase&, const Shader& shader, uint32_t numFrameBuffers,
                            const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader,
                            const std::function<const tPerFrameVkBuffer(const std::string&)>& bufferLoader,
                            const std::function<const ImageInfo(const std::string&)>& imageLoader = nullptr,
                            const std::function<const tPerFrameVkAccelerationStructure(const std::string&)>& accelerationStructureLoader = nullptr,
                            const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader = nullptr) const override;

    Material CreateMaterial(T_GFXAPI& gfxApi, const ShaderT<T_GFXAPI>& shader, uint32_t numFrameBuffers,
                             const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader,
                             const std::function<const tPerFrameVkBuffer(const std::string&)>& bufferLoader,
                             const std::function<const ImageInfo(const std::string&)>& imageLoader = nullptr,
                             const std::function<const tPerFrameVkAccelerationStructure(const std::string&)>& accelerationStructureLoader = nullptr,
                             const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader = nullptr) const;
protected:
    /// Internal material pass creation (does not UpdateDescriptorSets)
    MaterialPass CreateMaterialPassInternal(T_GFXAPI& vulkan,
                                             const ShaderPass<T_GFXAPI>& shaderPass,
                                             uint32_t numFrameBuffers,
                                             const std::function<const tPerFrameTexInfo(const std::string&)>& textureLoader,
                                             const std::function<const tPerFrameVkBuffer(const std::string&)>& bufferLoader,
                                             const std::function<const ImageInfo(const std::string&)>& imageLoader,
                                             const std::function<const tPerFrameVkAccelerationStructure(const std::string&)>& accelerationStructureLoader,
                                             const std::function<const VertexElementData(const std::string&)>& specializationStructureLoader,
                                             const std::string& passDebugName) const;
};
