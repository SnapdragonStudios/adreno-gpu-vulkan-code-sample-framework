//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <volk/volk.h>
#include "pipelineLayout.hpp"
#include "pipelineVertexInputState.hpp"
#include "specializationConstantsLayout.hpp"
#include "shaderModule.hpp"
#include "../shader.hpp"

// forward declarations
class DescriptorSetLayoutBase;
class ShaderDescription;
template<typename T_GFXAPI> class ShaderModule;
class ShaderPassDescription;
