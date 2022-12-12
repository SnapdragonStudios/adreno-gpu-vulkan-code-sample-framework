//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#include "postProcessStandard.hpp"
#include "imgui/imgui.h"
#include "material/materialManager.hpp"
#include "material/drawable.hpp"
#include "material/shader.hpp"
#include "system/os_common.h"
#include "vulkan/MeshObject.h"
#include "nlohmann/json.hpp"


PostProcessStandard::PostProcessStandard(Vulkan& vulkan) : m_Vulkan(vulkan)
{}

PostProcessStandard::~PostProcessStandard()
{
    ReleaseUniformBuffer(&m_Vulkan, m_FragUniform);
}

bool PostProcessStandard::Init(const Shader& shader, MaterialManager& materialManager, VkRenderPass blitRenderPass, tcb::span<const VulkanTexInfo> diffuseRenderTargets, VulkanTexInfo* bloomRenderTarget, VulkanTexInfo* uiRenderTarget)
{
    assert(!diffuseRenderTargets.empty());
    assert(bloomRenderTarget);
    assert(uiRenderTarget);

    //
    // Create the uniform buffers (for the blit fragment shader)
    if (!CreateUniformBuffer(&m_Vulkan, m_FragUniform, &m_FragUniformData))
        return false;

    //
    // Create the 'Blit' drawable mesh.
    // Fullscreen quad that does the final composite (hud and scene) for output.
    MeshObject blitMesh;
    MeshObject::CreateScreenSpaceMesh(&m_Vulkan, 0, &blitMesh);

    auto blitShaderMaterial = materialManager.CreateMaterial(m_Vulkan, shader, NUM_VULKAN_BUFFERS,
        [&](const std::string& texName) -> const MaterialManager::tPerFrameTexInfo {
            if (texName == "Diffuse") {
                return { diffuseRenderTargets.data() };
            }
            else if (texName == "Bloom") {
                return { bloomRenderTarget };
            }
            else if (texName == "Overlay") {
                return { uiRenderTarget };
            }
            assert(0);
            return {};
        },
        [this](const std::string& bufferName) -> MaterialManager::tPerFrameVkBuffer {
            //BlitFragCB
            return { m_FragUniform.vkBuffers.begin(), m_FragUniform.vkBuffers.end() };
        }
        );

    m_Drawable = std::make_unique<Drawable>(m_Vulkan, std::move(blitShaderMaterial));
    if (!m_Drawable->Init(blitRenderPass, "RP_BLIT", std::move(blitMesh)))
    {
        return false;
    }

    // Save off the render target pointers as we may need to update descriptor sets to point to them (inside update)
    m_DiffuseRenderTargets.clear();
    m_DiffuseRenderTargets.reserve(diffuseRenderTargets.size());
    for(const auto& renderTarget : diffuseRenderTargets)
        m_DiffuseRenderTargets.push_back(&renderTarget);

    return true;
}

bool PostProcessStandard::UpdateUniforms(uint32_t WhichFrame, float ElapsedTime)
{
    //m_FragUniformData.sRGB = m_bEncodeSRGB ? 1 : 0;
    UpdateUniformBuffer(&m_Vulkan, m_FragUniform, m_FragUniformData, WhichFrame);

    // If we have multiple diffuse inputs we need to cycle through them each frame.
    if (m_DiffuseRenderTargets.size() > 1)
    {
        m_Drawable->GetMaterial().UpdateDescriptorSetBinding(WhichFrame, "Diffuse", *m_DiffuseRenderTargets[m_CurrentRenderTargetIdx]);
        if (++m_CurrentRenderTargetIdx >= m_DiffuseRenderTargets.size())
            m_CurrentRenderTargetIdx = 0;
    }
    return true;
}

void PostProcessStandard::UpdateGui()
{
    ImGui::SliderFloat("Exposure", &m_FragUniformData.Exposure, 0.01f, 2.0f);
    ImGui::SliderFloat("Contrast", &m_FragUniformData.Contrast, 0.01f, 2.0f);
    ImGui::SliderFloat("Vignette", &m_FragUniformData.Vignette, 0.0f, 1.0f);
    bool filmic = m_FragUniformData.ACESFilmic != 0;
    if (ImGui::Checkbox("Filmic Tonemapper", &filmic))
        m_FragUniformData.ACESFilmic = filmic ? 1 : 0;
}


void to_json(Json& json, const PostProcessStandard& post)
{
    PostProcessStandard::to_json_int(json, post);
}
void PostProcessStandard::to_json_int(Json& j, const PostProcessStandard& post)
{
    j = Json{ {"Contrast", post.m_FragUniformData.Contrast},
              {"Exposure", post.m_FragUniformData.Exposure},
              {"Vignette", post.m_FragUniformData.Vignette},
              {"ACESFilmic", post.m_FragUniformData.ACESFilmic} };
}

void PostProcessStandard::Load(const Json& j)
{
    auto it = j.find("Contrast");
    if (it != j.end()) it->get_to(m_FragUniformData.Contrast);
    it = j.find("Exposure");
    if (it != j.end()) it->get_to(m_FragUniformData.Exposure);
    it = j.find("Vignette");
    if (it != j.end()) it->get_to(m_FragUniformData.Vignette);
    it = j.find("ACESFilmic");
    if (it != j.end()) it->get_to(m_FragUniformData.ACESFilmic);
}

void PostProcessStandard::Save(Json& json) const
{
    json = Json(*this);
}

const Drawable* const PostProcessStandard::GetDrawable() const
{
    return m_Drawable.get();
}

void PostProcessStandard::SetEncodeSRGB(bool encode)
{
    m_FragUniformData.sRGB = encode ? 1 : 0;
}
