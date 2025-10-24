//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "postProcess.hpp"
#include "vulkan/vulkan.hpp"
#include "memory/vulkan/uniform.hpp"
#include "system/glm_common.hpp"
#include <memory>

// Forward declarations
class ShaderBase;
template<typename T_GFXAPI> class Drawable;
template<typename T_GFXAPI> class MaterialManager;
template<typename T_GFXAPI> class RenderPass;
template<typename T_GFXAPI> class Shader;
template<typename T_GFXAPI> class Texture;


class PostProcessStandard : public PostProcess<Vulkan>
{
    PostProcessStandard& operator=(const PostProcessStandard&) = delete;
    PostProcessStandard(const PostProcessStandard&) = delete;
public:
    PostProcessStandard(Vulkan& vulkan);
    ~PostProcessStandard();

    bool Init(const Shader<Vulkan>& shader, MaterialManager<Vulkan>& materialManager, const RenderPass<Vulkan>& blitRenderPass, std::span<const Texture<Vulkan>> diffuseRenderTargets, Texture<Vulkan>* bloomRenderTarget, Texture<Vulkan>* uiRenderTarget);
    bool UpdateUniforms(uint32_t WhichFrame, float ElapsedTime) override;
    void UpdateGui() override;

    void Load(const Json&) override;
    void Save(Json& json) const override;

    const Drawable*const GetDrawable() const override;

    void SetEncodeSRGB(bool encode);    ///< enable/disable encoding of output to sRGB color space (should depend on final buffer format, ideally have sRGB final buffer).

protected:
    Vulkan& m_Vulkan;

    std::unique_ptr<Drawable>   m_Drawable;

    struct BlitFragUB {
        float Bloom = 0.0f;

        float Exposure = 1.0f;  // Scene exposure
        float Contrast = 1.0f;  // Scene contrast
        float Vignette = 0.0f;  // Amount of vignette texture to apply
        int ACESFilmic = true;  // 1 - apply ACES Filmic tonemapping (0 = dont)
        int sRGB = 0;           // 1 - apply srgb conversion in output blit shader, 0 passthrough color
    } m_FragUniformData;
    UniformArrayT<Vulkan, BlitFragUB, NUM_VULKAN_BUFFERS> m_FragUniform;
    std::vector<Texture<Vulkan> const*> m_DiffuseRenderTargets;
    size_t m_CurrentRenderTargetIdx = 0;

private:
    friend void to_json(Json& json, const PostProcessStandard& );
    static void to_json_int(Json& json, const PostProcessStandard& );
};
