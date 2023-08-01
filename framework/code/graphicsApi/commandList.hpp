//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Forward declarations
template<class T_GFXAPI> class CommandListT;


// Base graphics Command list container class
class CommandList
{
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
public:
    CommandList() noexcept {}
    ~CommandList() {}

    template<class T_GFXAPI> using tApiDerived = CommandListT<T_GFXAPI>; // make apiCast work!
};

// Templated command list container class (templated against graphics api)
// Expected for this class to be specialized for each graphics api.
template<class T_GFXAPI>
class CommandListT : public CommandList
{
    CommandListT(const CommandListT<T_GFXAPI>&) = delete;
    CommandListT& operator=(const CommandListT<T_GFXAPI>&) = delete;
public:
    CommandListT() noexcept {}      // This class is expected to be specialized!
    ~CommandListT() = delete;       // This class is expected to be specialized!

protected:
    static_assert(sizeof(CommandListT<T_GFXAPI>) != sizeof(CommandList));   // Ensure this class template is specialized (and not used as-is)
};

