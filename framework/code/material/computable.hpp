//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <map>
#include <memory>
#include <array>
#include <vector>
#include <string>
#include "shader.hpp"
#include "material.hpp"


// Forward Declarations
class CommandListBase;
class MaterialBase;
class MaterialPassBase;
template<typename T_GFXAPI> class Computable;
template<typename T_GFXAPI> class ComputablePass;
template<typename T_GFXAPI> class Material;
template<typename T_GFXAPI> class CommandList;


class ImageMemoryBarrierBase
{
protected:
    ImageMemoryBarrierBase() noexcept = default;
    // ok to copy (default copy operators!)
};

template<class T_GFXAPI>
class ImageMemoryBarrier final : public ImageMemoryBarrierBase
{
    ImageMemoryBarrier( const ImageMemoryBarrier& ) = delete;
    ImageMemoryBarrier& operator=( const ImageMemoryBarrier& ) = delete;
    ImageMemoryBarrier() = delete;
    ~ImageMemoryBarrier() = delete;
    static_assert(sizeof( ImageMemoryBarrier<T_GFXAPI> ) != sizeof( ImageMemoryBarrierBase ));  // static assert if not specialized
};

class BufferMemoryBarrierBase
{
protected:
    BufferMemoryBarrierBase() noexcept = default;
    // ok to copy (default copy operators!)
};

template<class T_GFXAPI>
class BufferMemoryBarrier final : public BufferMemoryBarrierBase
{
    BufferMemoryBarrier( const BufferMemoryBarrier& ) = delete;
    BufferMemoryBarrier& operator=( const BufferMemoryBarrier& ) = delete;
    BufferMemoryBarrier() = delete;
    ~BufferMemoryBarrier() = delete;
    static_assert(sizeof( BufferMemoryBarrier<T_GFXAPI> ) != sizeof( BufferMemoryBarrierBase ));  // static assert if not specialized
};

/// Encapsulates a 'computable' pass, contains the materialpass, pipeline, etc).
/// Similar to a DrawablePass but without the vertex buffer
/// @ingroup Material
class ComputablePassBase
{
    ComputablePassBase(const ComputablePassBase&) = delete;
    ComputablePassBase& operator=(const ComputablePassBase&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = ComputablePass<T_GFXAPI>; // make apiCast work!
    /// number of workgroup dispatches to execute (value after the local workgroup sizes are accounted for)
    void SetDispatchGroupCount( std::array<uint32_t, 3> count )     { mDispatchGroupCount = count; }
    const auto& GetDispatchGroupCount() const                       { return mDispatchGroupCount; }
    /// number of global workgroup threads to run (value before the local workgroup sizes are accounted for).  Requires "WorkGroup" : { "LocalSize": {x,y,z} } in the shader definition json.
    void SetDispatchThreadCount( std::array<uint32_t, 3> count );

protected:
    ComputablePassBase( const MaterialPassBase& materialPass ) noexcept
        : mMaterialPass(materialPass)
    {}
    ComputablePassBase( ComputablePassBase&& ) noexcept;

    const MaterialPassBase&             mMaterialPass;
    std::array<uint32_t, 3>             mDispatchGroupCount{1u,1u,1u};
};


template<class T_GFXAPI>
class ComputablePass final : public ComputablePassBase
{
    ComputablePass() = delete;
    ~ComputablePass() = delete;
    static_assert(sizeof( ComputablePass<T_GFXAPI> ) != sizeof( ComputablePassBase ));  // static assert if not specialized
};


/// Encapsulates a 'computable' object, contains the MaterialBase, computable passes.
/// Similar to a Drawable but without the vertex buffer
class ComputableBase
{
    ComputableBase(const ComputableBase&) = delete;
    ComputableBase& operator=(const ComputableBase&) = delete;
protected:
    ComputableBase() noexcept {}
public:
    template<typename T_GFXAPI> using tApiDerived = Computable<T_GFXAPI>; // make apiCast work!
    virtual ~ComputableBase() {}

    /// @brief  Initialize the computable (call once, prior to dispatching)
    /// @return true on success
    virtual bool Init() = 0;

    /// Return the pass name for the given compute pass index
    virtual const std::string& GetPassName( uint32_t passIdx ) const = 0;

    /// number of workgroup dispatches to execute (value after the local workgroup sizes are accounted for)
    virtual void SetDispatchGroupCount(uint32_t passIdx, const std::array<uint32_t, 3>& groupCount) = 0;
    /// number of global workgroup threads to run (value before the local workgroup sizes are accounted for).  Requires "WorkGroup" : { "LocalSize": {x,y,z} } in the shader definition json.
    virtual void SetDispatchThreadCount(uint32_t passIdx, const std::array<uint32_t, 3>& threadCount) = 0;

    /// Add all the commands to cmdList needed to 'dispatch' all the passes contained in this Computable
    /// Will add necissary barriers before the first dispatch, between the passes and will bind the appropriate descriptor sets needed by each.
    virtual void Dispatch(CommandListBase& commandList, uint32_t bufferIdx, bool addTimers) const = 0;

    /// Add the commands to barrier / syncronize all buffers written by the computable.
    virtual void AddOutputBarriersToCmdList(CommandListBase& commandList) const = 0;
};


template<class T_GFXAPI>
class Computable final : public ComputableBase
{
    Computable(const Computable&) = delete;
    Computable& operator=(const Computable&) = delete;

public:
    Computable(T_GFXAPI&, Material<T_GFXAPI>&&) noexcept;
    Computable(T_GFXAPI&, std::unique_ptr<MaterialBase> material) noexcept;
    Computable(Computable<T_GFXAPI>&&) noexcept;
    ~Computable() override;

    /// @brief  Initialize the computable (call once, prior to dispatching)
    /// @return true on success
    bool Init() override;

    const auto& GetPasses() const { return mPasses; }
    const auto& GetImageInputMemoryBarriers() const { return mImageInputMemoryBarriers; }
    const auto& GetImageOutputMemoryBarriers() const { return mImageOutputMemoryBarriers; }
    const auto& GetBufferOutputMemoryBarriers() const { return mBufferOutputMemoryBarriers; }

    /// Return the pass name for the given compute pass index
    const std::string& GetPassName( uint32_t passIdx ) const override;

    /// number of workgroup dispatches to execute (value after the local workgroup sizes are accounted for)
    void SetDispatchGroupCount(uint32_t passIdx, const std::array<uint32_t, 3>& groupCount) override;
    /// number of global workgroup threads to run (value before the local workgroup sizes are accounted for).  Requires "WorkGroup" : { "LocalSize": {x,y,z} } in the shader definition json.
    void SetDispatchThreadCount(uint32_t passIdx, const std::array<uint32_t, 3>& threadCount) override;

    /// Add all the commands to the cmdBuffer needed to 'dispatch' the given pass.
    /// Will add necissary barriers before the dispatch, along with binding the appropriate descriptor sets needed by the pass.
    void DispatchPass(CommandList<T_GFXAPI>& cmdList, const ComputablePass<T_GFXAPI>& computablePass, uint32_t bufferIdx) const;

    /// Add all the commands to cmdList needed to 'dispatch' all the passes contained in this Computable
    /// Will add necissary barriers before the first dispatch, between the passes and will bind the appropriate descriptor sets needed by each.
    void Dispatch(CommandListBase& commandList, uint32_t bufferIdx, bool addTimers) const override;
    void Dispatch(CommandList<T_GFXAPI>& commandList, uint32_t bufferIdx, bool addTimers) const;

    /// Add the commands to barrier / syncronize all buffers written by the computable.
    void AddOutputBarriersToCmdList(CommandListBase& commandList) const override;
    void AddOutputBarriersToCmdList(CommandList<T_GFXAPI>& commandList) const;

protected:
    T_GFXAPI&                                   mGfxApi;
    Material<T_GFXAPI>                          mMaterial;
    std::vector<ComputablePass<T_GFXAPI>>       mPasses;
    std::vector<ImageMemoryBarrier<T_GFXAPI>>   mImageInputMemoryBarriers;      // barriers for ENTRY to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::vector<ImageMemoryBarrier<T_GFXAPI>>   mImageOutputMemoryBarriers;     // barriers for EXIT to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
    std::vector<BufferMemoryBarrier<T_GFXAPI>>  mBufferOutputMemoryBarriers;    // barriers for EXIT to this computable.  Could be stored in material if non compute shaders wanted this barrier information, non compute should use the pass depenancies tho
};


template<class T_GFXAPI>
Computable<T_GFXAPI>::Computable(T_GFXAPI& gfxApi, Material<T_GFXAPI>&& material) noexcept
    : ComputableBase()
    , mGfxApi(gfxApi)
    , mMaterial(std::move(material))
{
}

template<typename T_GFXAPI>
Computable<T_GFXAPI>::Computable(T_GFXAPI& gfxapi, std::unique_ptr<MaterialBase> material) noexcept
    : ComputableBase()
    , mGfxApi( gfxapi )
    , mMaterial(std::move( *apiCast<T_GFXAPI>( material.get() ) ))
{
}

template<class T_GFXAPI>
Computable<T_GFXAPI>::Computable(Computable<T_GFXAPI>&& other) noexcept
    : mGfxApi(other.mGfxApi)
    , mMaterial(std::move(other.mMaterial))
    , mPasses(std::move(other.mPasses))
    , mImageInputMemoryBarriers(std::move(other.mImageInputMemoryBarriers))
    , mImageOutputMemoryBarriers(std::move(other.mImageOutputMemoryBarriers))
    , mBufferOutputMemoryBarriers(std::move(other.mBufferOutputMemoryBarriers))
{
}

template<class T_GFXAPI>
Computable<T_GFXAPI>::~Computable()
{
}

template<class T_GFXAPI>
const std::string& Computable<T_GFXAPI>::GetPassName( uint32_t passIdx ) const
{
    const auto& passNames = static_cast<const MaterialBase&>(mMaterial).GetShader().GetShaderPassIndicesToNames();
    assert( passIdx <= passNames.size() );
    return passNames[passIdx];
}

