//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <string>
#include <memory>
#include <functional>
#include "material.hpp"

// Forward declarations
class GraphicsApiBase;
class Vulkan;
class MaterialPassBase;
class ShaderDescription;
class ShaderBase;
template<typename T_GFXAPI> class Material;
template<typename T_GFXAPI> class MaterialManager;
template<typename T_GFXAPI> class MaterialPass;
template<typename T_GFXAPI> class Shader;
template<typename T_GXFAPI> class ShaderPass;
class AccelerationStructureBase;
class TextureBase;
template<typename T_GXFAPI> struct ImageInfo;
struct ImageInfoBase;
struct PerFrameBufferBase;
template<typename T_GXFAPI> struct PerFrameBuffer;
class VertexElementData;


/// Helper class for creating MaterialBase (base class, expected for there to be a graphics api specific derived class)
/// @ingroup Material
class MaterialManagerBase
{
    MaterialManagerBase(const MaterialManagerBase&) = delete;
    MaterialManagerBase& operator=(const MaterialManagerBase&) = delete;
protected:
    MaterialManagerBase(GraphicsApiBase& gfxApi) noexcept
        : mGfxApi(gfxApi)
    {}

public:
    typedef std::vector<const TextureBase*> tPerFrameTexInfo;
    typedef std::vector<const AccelerationStructureBase*> tPerFrameAccelerationStructure;

    virtual std::unique_ptr<MaterialBase> CreateMaterial(const ShaderBase& shader,
                                                     uint32_t numFrameBuffers,
                                                     const std::function<const void( const std::string&, tPerFrameTexInfo& )>& textureLoader,
                                                     const std::function<const void( const std::string&, PerFrameBufferBase& )>& bufferLoader,
                                                     const std::function<const void( const std::string&, ImageInfoBase& )>& imageLoader = nullptr,
                                                     const std::function<const tPerFrameAccelerationStructure(const std::string&)>& accelerationStructureLoader = nullptr,
                                                     const std::function<const VertexElementData(const std::string&)>& specializationConstantLoader = nullptr) const = 0;
    GraphicsApiBase &mGfxApi;
};


/// Helper class for creating MaterialBase 
/// @ingroup Material
template<typename T_GFXAPI>
class MaterialManager : public MaterialManagerBase
{
    MaterialManager<T_GFXAPI>& operator=(const MaterialManager<T_GFXAPI>&) = delete;
    MaterialManager(const MaterialManager<T_GFXAPI>&) = delete;
public:

    MaterialManager( T_GFXAPI& gfxApi ) noexcept;
    ~MaterialManager();

    virtual std::unique_ptr<MaterialBase> CreateMaterial( const ShaderBase& shader,
                                                      uint32_t numFrameBuffers,
                                                      const std::function<const void( const std::string&, tPerFrameTexInfo& )>& textureLoader,
                                                      const std::function<const void( const std::string&, PerFrameBufferBase& )>& bufferLoader,
                                                      const std::function<const void( const std::string&, ImageInfoBase& )>& imageLoader = nullptr,
                                                      const std::function<const tPerFrameAccelerationStructure( const std::string& )>& accelerationStructureLoader = nullptr,
                                                      const std::function<const VertexElementData( const std::string& )>& specializationConstantLoader = nullptr ) const override final;

    Material<T_GFXAPI> CreateMaterial(  const Shader<T_GFXAPI>& shader,
                                        uint32_t numFrameBuffers,
                                        const std::function<const tPerFrameTexInfo( const std::string& )>& textureLoader,
                                        const std::function<const PerFrameBuffer<T_GFXAPI>( const std::string& )>& bufferLoader,
                                        const std::function<const ImageInfo<T_GFXAPI>( const std::string& )>& imageLoader = nullptr,
                                        const std::function<const tPerFrameAccelerationStructure( const std::string& )>& accelerationStructureLoader = nullptr,
                                        const std::function<const VertexElementData( const std::string& )>& specializationConstantLoader = nullptr ) const;
    //{
    //    static_assert(sizeof( T_GFXAPI ) != sizeof( T_GFXAPI ), "Must use the specialized version of this function.  Your are likely missing #include \"material/<GFXAPI>/materialManager.hpp\"");
    //}
protected:

    /// Internal material pass creation (does not UpdateDescriptorSets)
    MaterialPass<T_GFXAPI> CreateMaterialPassInternal(  const ShaderPass<T_GFXAPI>& shaderPass,
                                                        uint32_t numFrameBuffers,
                                                        const std::function<const tPerFrameTexInfo( const std::string& )>& textureLoader,
                                                        const std::function<const PerFrameBuffer<T_GFXAPI>( const std::string& )>& bufferLoader,
                                                        const std::function<const ImageInfo<T_GFXAPI>( const std::string& )>& imageLoader,
                                                        const std::function<const tPerFrameAccelerationStructure( const std::string& )>& accelerationStructureLoader,
                                                        const std::function<const VertexElementData( const std::string& )>& specializationStructureLoader,
                                                        const std::string& passDebugName ) const;
    //{
    //    static_assert(sizeof( T_GFXAPI ) != sizeof( T_GFXAPI ), "Must use the specialized version of this function.  Your are likely missing #include \"material/<GFXAPI>/materialManager.hpp\"");
    //}

    //static_assert(sizeof( MaterialManager<T_GFXAPI>) != sizeof(MaterialManagerBase));  // Expecting that this template be specialized.  If you get a compile error here maybe you didnt #include the materialManager.hpp for the gfxapi (eg MaterialBase\Vulkan\materialManager.hpp)
};

template<typename T_GFXAPI>
std::unique_ptr<MaterialBase> MaterialManager<T_GFXAPI>::CreateMaterial( const ShaderBase& shader,
                                                                      uint32_t numFrameBuffers,
                                                                      const std::function<const void( const std::string&, tPerFrameTexInfo& )>& textureLoader,
                                                                      const std::function<const void( const std::string&, PerFrameBufferBase&   )>& bufferLoader,
                                                                      const std::function<const void( const std::string&, ImageInfoBase&    )>& imageLoader,
                                                                      const std::function<const tPerFrameAccelerationStructure( const std::string& )>& accelerationStructureLoader,
                                                                      const std::function<const VertexElementData( const std::string& )>& specializationConstantLoader ) const
{
    auto textureLoader2 = [&textureLoader]( const std::string& name ) ->tPerFrameTexInfo {
        tPerFrameTexInfo texInfo;
        textureLoader( name, texInfo );
        return texInfo;
        };
    auto bufferLoader2 = [&bufferLoader]( const std::string& name ) -> PerFrameBuffer<T_GFXAPI> {
        PerFrameBuffer<T_GFXAPI> buffers;
        bufferLoader( name, buffers );
        return buffers;
    };
    auto imageLoader2 = [&imageLoader]( const std::string& name ) ->ImageInfo<T_GFXAPI> {
        ImageInfo<T_GFXAPI> imageInfo;
        imageLoader( name, imageInfo );
        return imageInfo;
        };

    return std::make_unique< Material<T_GFXAPI>>(
        MaterialManager<T_GFXAPI>::CreateMaterial(  apiCast<T_GFXAPI>( shader ),
                                                    numFrameBuffers,
                                                    textureLoader2,
                                                    bufferLoader2,
                                                    imageLoader2,
                                                    accelerationStructureLoader,
                                                    specializationConstantLoader ) );
}
