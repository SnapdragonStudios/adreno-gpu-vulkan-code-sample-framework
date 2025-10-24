//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Forward declarations
class GraphicsApiBase;
template<class T_GFXAPI> class AccelerationStructure;


/// @brief Base class for a acceleration structure object.
/// Is subclassed for each graphics API.
class AccelerationStructureBase
{
    AccelerationStructureBase( const AccelerationStructureBase& ) = delete;
    AccelerationStructureBase& operator=( const AccelerationStructureBase& ) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = AccelerationStructure<T_GFXAPI>; // make apiCast work!

    AccelerationStructureBase() noexcept {}
    virtual ~AccelerationStructureBase() noexcept = 0;
};
inline AccelerationStructureBase::~AccelerationStructureBase() noexcept {}


template<typename T_GFXAPI>
class AccelerationStructure final : public AccelerationStructureBase
{
    AccelerationStructure( const AccelerationStructure<T_GFXAPI>& ) = delete;
    AccelerationStructure& operator=( const AccelerationStructure<T_GFXAPI>& ) = delete;
public:
    AccelerationStructure() noexcept = delete;                                                         // template class expected to be specialized
    AccelerationStructure( AccelerationStructure<T_GFXAPI>&& ) noexcept = delete;                     // template class expected to be specialized
    AccelerationStructure<T_GFXAPI>& operator=( AccelerationStructure<T_GFXAPI>&& ) noexcept = delete;// template class expected to be specialized

    bool IsEmpty() const { return true; }

protected:
    static_assert(sizeof( AccelerationStructure<T_GFXAPI> ) != sizeof( AccelerationStructureBase ));       // Ensure this class template is specialized (and not used as-is).  Include the vulkan / dx12 (or whatever gfx api) specific header file
};
