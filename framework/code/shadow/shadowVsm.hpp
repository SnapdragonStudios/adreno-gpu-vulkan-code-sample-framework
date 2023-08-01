//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <memory>

class GraphicsApiBase;
class Computable;
class MaterialManager;
class ShaderManager;
class Shadow;
class Texture;
class CommandList;

/// Variance Shadow Map
class ShadowVSM
{
    ShadowVSM(const ShadowVSM&) = delete;
    ShadowVSM& operator=(const ShadowVSM&) = delete;
public:
    ShadowVSM();

    bool Initialize(GraphicsApiBase& , const ShaderManager& , const MaterialManager& , const Shadow& );
    void Release(GraphicsApiBase&);

    const Texture* const GetVSMTexture() const { return m_VsmTarget.get(); }
    void AddComputableToCmdBuffer(CommandList& cmdBuffer);

protected:
    std::unique_ptr<Computable>     m_Computable;
    std::unique_ptr<Texture>        m_VsmTarget;
    std::unique_ptr<Texture>        m_VsmTargetIntermediate;
};
