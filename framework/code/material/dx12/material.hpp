//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "dx12/dx12.hpp"
#include "materialPass.hpp"
#include "../material.hpp"
#include "../materialT.hpp"

using MaterialDx12 = Material<Dx12>;


/// @defgroup Material
/// Material and Shader loader.
/// Handles creation of descriptor sets, buffer binding, shader binaries (everything in Dx12 that describes and is used to render a shader).
/// 
/// Typically user (application) writes a Json description for each 'Shader' that describes the inputs, outputs, and internal state of the shader, and the shader code (glsl).
/// The user then uses ShaderManagerBase::AddShader to register (and load) each Json file and the shader binaries.
/// 
/// From there the user uses MaterialManagerBase::CreateMaterial to create a MaterialBase instance of the Shader (a MaterialBase contains bindings to the various texture/buffer inputs that a Shader requires - there can be many Materials using the same Shader (each MaterialBase with different textures, vertex buffers and/or uniform buffers etc)
/// 
/// The MaterialBase returned by CreateMaterial can be used to Create a Drawable or Computable object that wraps everything together with one convenient interface!
/// 
/// For more complex models the user should use DrawableLoader::LoadDrawable to load the mesh model file (and return a vector of Drawables).  This api greatly simplifies the material creation and binding, splitting model meshes across material boundaries, automatically detecting instances (optionally).
/// 
