//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <memory>

class GraphicsApiBase;
class ComputableBase;
class MaterialManagerBase;
class ShaderManagerBase;
class Shadow;
class TextureBase;
class CommandListBase;

template<class T_GFXAPI> class MaterialManager;
template<class T_GFXAPI> class ShaderManager;
template<class T_GFXAPI> class ShadowT;

/// Variance Shadow Map
class ShadowVSM
{
    ShadowVSM(const ShadowVSM&) = delete;
    ShadowVSM& operator=(const ShadowVSM&) = delete;
public:
    ShadowVSM();

    template<class T_GFXAPI>
    bool Initialize(T_GFXAPI& , const ShaderManager<T_GFXAPI>& , const MaterialManager<T_GFXAPI>& , const ShadowT<T_GFXAPI>& );
    void Release(GraphicsApiBase&);

    const TextureBase* const GetVSMTexture() const { return m_VsmTarget.get(); }
    void AddComputableToCmdBuffer(CommandListBase& cmdBuffer);

protected:
    std::unique_ptr<ComputableBase> m_Computable;
    std::unique_ptr<TextureBase>        m_VsmTarget;
    std::unique_ptr<TextureBase>        m_VsmTargetIntermediate;
};
