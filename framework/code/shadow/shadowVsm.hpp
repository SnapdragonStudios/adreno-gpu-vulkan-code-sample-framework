//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <memory>

class Vulkan;
class Computable;
class MaterialManager;
class ShaderManager;
class Shadow;
class VulkanTexInfo;
class Wrap_VkCommandBuffer;

/// Variance Shadow Map
class ShadowVSM
{
    ShadowVSM(const ShadowVSM&) = delete;
    ShadowVSM& operator=(const ShadowVSM&) = delete;
public:
    ShadowVSM();

    bool Initialize(Vulkan& , const ShaderManager& , const MaterialManager& , const Shadow& );
    void Release(Vulkan* vulkan);

    const VulkanTexInfo* const GetVSMTexture() const { return m_VsmTarget.get(); }
    void AddComputableToCmdBuffer(Wrap_VkCommandBuffer& cmdBuffer);

protected:
    std::unique_ptr<Computable>     m_Computable;
    std::unique_ptr<VulkanTexInfo>  m_VsmTarget;
    std::unique_ptr<VulkanTexInfo>  m_VsmTargetIntermediate;
};
