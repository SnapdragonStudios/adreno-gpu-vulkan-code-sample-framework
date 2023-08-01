//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
class Drawable;
class Shader;
template<typename T_GFXAPI> class MaterialManagerT;
template<typename T_GFXAPI> class TextureT;


class PostProcessStandard : public PostProcess
{
    PostProcessStandard& operator=(const PostProcessStandard&) = delete;
    PostProcessStandard(const PostProcessStandard&) = delete;
public:
    PostProcessStandard(Vulkan& vulkan);
    ~PostProcessStandard();

    bool Init(const Shader& shader, MaterialManagerT<Vulkan>& materialManager, VkRenderPass blitRenderPass, std::span<const TextureT<Vulkan>> diffuseRenderTargets, TextureT<Vulkan>* bloomRenderTarget, TextureT<Vulkan>* uiRenderTarget);
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
    std::vector<TextureT<Vulkan> const*> m_DiffuseRenderTargets;
    size_t m_CurrentRenderTargetIdx = 0;

private:
    friend void to_json(Json& json, const PostProcessStandard& );
    static void to_json_int(Json& json, const PostProcessStandard& );
};
