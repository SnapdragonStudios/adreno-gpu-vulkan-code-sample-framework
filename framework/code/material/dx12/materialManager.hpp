//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "../materialManager.hpp"

class Dx12;


///// @brief Template specialization of MaterialManager container for Dx12 graphics api.
//template<>
//class MaterialManager<Dx12> final : public MaterialManagerBase
//{
//    using ImageInfo = ImageInfo<Dx12>;
//    MaterialManager<Dx12>& operator=( const MaterialManager<Dx12>& ) = delete;
//    MaterialManager( const MaterialManager<Dx12>& ) = delete;
//public:
//
//    MaterialManager();
//    ~MaterialManager();
//
//    /// Create a material to be used for rendering (potentially contains multiple passes)
//    /// If numFrameBuffers is not 1 there will be numFrameBuffers descriptor sets created (so different buffers can be bound on different 'frames', although the layout is fixed)
//    Material<Dx12> CreateMaterial( Dx12& gfxApi, const Shader<Dx12>& shader, uint32_t numFrameBuffers,
//                             const std::function<const tPerFrameTexInfo( const std::string& )>& textureLoader,
//                             const std::function<const PerFrameBuffer<Dx12>( const std::string& )>& bufferLoader,
//                             const std::function<const ImageInfo( const std::string& )>& imageLoader = nullptr,
//                             const std::function<const tPerFrameAccelerationStructure( const std::string& )>& accelerationStructureLoader = nullptr,
//                             const std::function<const VertexElementData( const std::string& )>& specializationConstantLoader = nullptr ) const;
//protected:
//    /// Internal material pass creation (does not UpdateDescriptorSets)
//    MaterialPass<Dx12> CreateMaterialPassInternal( Dx12& gfxapi,
//                                             const ShaderPass<Dx12>& shaderPass,
//                                             uint32_t numFrameBuffers,
//                                             const std::function<const tPerFrameTexInfo( const std::string& )>& textureLoader,
//                                             const std::function<const PerFrameBuffer<Dx12>( const std::string& )>& bufferLoader,
//                                             const std::function<const ImageInfo( const std::string& )>& imageLoader,
//                                             const std::function<const tPerFrameAccelerationStructure( const std::string& )>& accelerationStructureLoader,
//                                             const std::function<const VertexElementData( const std::string& )>& specializationStructureLoader,
//                                             const std::string& passDebugName ) const;
//};
