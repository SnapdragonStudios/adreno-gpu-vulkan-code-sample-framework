//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#if defined(GLM_SETUP_INCLUDED)
#error "Do not include glm.hpp 'loose', instead use this header so that glm defines are configured correctly"
#endif
#if defined(_SYS_CONFIG_H_)
#error "glm_common.hpp must be included before config.h (for glm::vec* deserialization functionality)"
#endif

// GLM Include Files
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_CXX17
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/fwd.hpp>
