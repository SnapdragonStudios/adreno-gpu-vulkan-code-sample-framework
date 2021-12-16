// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "vulkan/vulkan.hpp"

#include "system/os_common.h"

#include "vulkan/TextureFuncts.h"
#include "vulkan/vulkan_support.hpp"

// GLM Include Files
#define GLmFORCE_CXX03
#define GLmDEPTH_ZERO_TO_ONE

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>


// MRTs have arrays of blend states, color layers, etc.
#define MAX_GMEM_OUTPUT_LAYERS              8

// Number of "sampler2D", "samplerCube", etc. that can be bound in material
#define MAX_MATERIAL_SAMPLERS               16

//=============================================================================
// Material Description
//=============================================================================

typedef struct _MaterialProps
{
    // Used by the application, not the engine.  Nice to have it here
    uint32_t                RenderPassMask;

    // Material Flags
    // Need one per render target layer.  Usually this will be one, but not for MRTs
    uint32_t                            NumBlendStates;
    VkPipelineColorBlendAttachmentState BlendStates[MAX_GMEM_OUTPUT_LAYERS];

    // Depth Test/Write
    VkBool32                DepthTestEnable;
    VkBool32                DepthWriteEnable;

    // Depth Bias
    VkBool32                DepthBiasEnable;
    float                   DepthBiasConstant;
    float                   DepthBiasClamp;
    float                   DepthBiasSlope;

    // Culling
    VkCullModeFlagBits      CullMode;

    // The Shader (responsibility of owner to clean up)
    ShaderInfo*             pShader;

    // Textures (responsibility of owner to clean up)
    VulkanTexInfo*          pTexture[MAX_MATERIAL_SAMPLERS];

    // Constant Buffers
    uint32_t                VertUniformOffset;
    uint32_t                VertUniformLength;
    Uniform*                pVertUniform;

    uint32_t                FragUniformOffset;
    uint32_t                FragUniformLength;
    Uniform*                pFragUniform;

    // Vulkan Objects
    VkDescriptorPool        DescPool;
    VkDescriptorSet         DescSet;

    VkDescriptorSetLayout   DescLayout;
    VkPipelineLayout        PipelineLayout;

    VkPipelineCache         PipelineCache;
    VkPipeline              Pipeline;
} MaterialProps;




