//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include <stdint.h>
#include "json/include/nlohmann/json_fwd.hpp"

// Forward declarations
class Vulkan;
class ComputableBase;
template<typename T_GFXAPI> class Drawable;
using Json = nlohmann::json;

/// @brief Base class for Post Process classes.
/// Roughly defines interface for a post processing class that does the final compositing of output image (generally using a fullscreen shader pass).
///
template<typename T_GFXAPI>
class PostProcess
{
protected:
    using Drawable = Drawable<T_GFXAPI>;
    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;
    PostProcess() noexcept = default;
public:
    virtual ~PostProcess() = 0;

    virtual void Load(const Json&) {}
    virtual void Save(Json& json) const {}

    virtual bool UpdateUniforms(uint32_t WhichFrame, float ElapsedTime) = 0;
    virtual void UpdateGui() {};

    virtual const Drawable* const GetDrawable() const { return nullptr; }
    virtual const ComputableBase* const GetComputable() const { return nullptr; }
};

template<typename T_GFXAPI>
inline PostProcess< T_GFXAPI>::~PostProcess() {}
