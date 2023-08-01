//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <vulkan/vulkan.h>
#include "material/shader.hpp"
#include "pipelineLayout.hpp"
#include "pipelineVertexInputState.hpp"
#include "material/specializationConstantsLayout.hpp"

// forward declarations
class DescriptorSetLayout;
class ShaderDescription;
template<typename T_GFXAPI> class ShaderModuleT;
class ShaderPassDescription;
