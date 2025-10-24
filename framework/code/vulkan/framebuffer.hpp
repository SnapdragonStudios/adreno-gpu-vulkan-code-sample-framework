//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <volk/volk.h>
#include <span>
#include <string>
#include <vector>
#include "refHandle.hpp"

// Forward declarations
template<typename T_GFXAPI> class RenderPass;
template<typename T_GFXAPI> class Texture;
class Vulkan;


/// Simple wrapper around VkFramebuffer (no DirectX equivalent).
/// Simplifies creation (and checks for leaks on destruction - is up to the owner to call Destroy)
/// This template class expected to be specialized (if this template throws compiler errors then the code is not using the specialization classes which is an issue!)
/// @ingroup Material
template<typename T_GFXAPI>
class Framebuffer
{
    Framebuffer& operator=( const Framebuffer<T_GFXAPI>& ) = delete;
    Framebuffer( const Framebuffer<T_GFXAPI>& ) = delete;
public:
    Framebuffer() noexcept = delete;
    Framebuffer( Framebuffer<T_GFXAPI>&& ) noexcept = delete;
    ~Framebuffer() = delete;

    static_assert(sizeof( Framebuffer<T_GFXAPI> ) >= 1);   // Ensure this class template is specialized (and not used as-is)
};


//=============================================================================
// RenderpassClearData
//=============================================================================
class RenderPassClearData
{
    RenderPassClearData( const RenderPassClearData& ) = default;    // private but usable from Copy()
    RenderPassClearData& operator=( const RenderPassClearData& ) = default;
public:
    using Texture = Texture<Vulkan>;
    RenderPassClearData() noexcept = default;
    RenderPassClearData( VkRect2D _scissor, VkViewport _viewport, std::vector<VkClearValue> _clearValues )
        : scissor{_scissor}
        , viewport{_viewport}
        , clearValues{std::move( _clearValues )}
    {
    }

    RenderPassClearData( RenderPassClearData&& ) noexcept = default;
    RenderPassClearData& operator=( RenderPassClearData&& ) = default;

    bool Initialize( const std::span<const Texture> ColorAttachments,
                     const Texture* pDepthAttachment );

    RenderPassClearData Copy() const {
        return *this;   // use the 'private' copy operator
    }

    VkRect2D                    scissor {};
    VkViewport                  viewport {};
    std::vector<VkClearValue>   clearValues;
};


//=============================================================================
// Framebuffer<Vulkan>
//=============================================================================

/// Container for a framebuffer.
template<>
class Framebuffer<Vulkan> final
{
public:
    using Texture = Texture<Vulkan>;
    using RenderPass = RenderPass<Vulkan>;
    using RefVkFramebuffer = RefHandle<VkFramebuffer>;
    Framebuffer();
    ~Framebuffer() = default;
    Framebuffer( const Framebuffer<Vulkan>& );
    Framebuffer& operator=( const Framebuffer<Vulkan>& );
    Framebuffer( Framebuffer<Vulkan>&& ) noexcept = default;
    Framebuffer& operator=( Framebuffer<Vulkan>&& ) noexcept = default;
    Framebuffer(RefVkFramebuffer framebuffer, VkRect2D renderArea, VkViewport viewport, std::vector<VkClearValue> clearValues, std::string name) noexcept
        : m_FrameBuffer( std::move(framebuffer))
        , m_Name(std::move(name))
        , m_RenderPassClearData{renderArea, viewport, std::move(clearValues)}
    {}
    Framebuffer( RefVkFramebuffer framebuffer, RenderPassClearData clearData, std::string name ) noexcept
        : m_FrameBuffer( std::move( framebuffer ) )
        , m_Name( std::move( name ) )
        , m_RenderPassClearData{std::move( clearData )}
    {}
    operator bool() const { return !!m_FrameBuffer; }

    /// @brief Initialize the framebuffer with (potentially) multiple color buffers (including depth and resolve buffers where applicable
//    bool Initialize( Vulkan* pVulkan, const RenderTargetInitializeInfo& info, const char* pName );
    bool Initialize( Vulkan& vulkan,
                     const RenderPass& renderPass,
                     const std::span<const Texture> ColorAttachments,
                     const Texture* pDepthAttachment,
                     std::string name,
                     const std::span<const Texture> ResolveAttachments = {},
                     const Texture* pVRSAttachment = nullptr);
    void Release();
    operator VkFramebuffer() const { return m_FrameBuffer; }
    const auto& GetRenderPassClearData() const { return m_RenderPassClearData; }   ///< default clear data and framebuffer extents

    // Attributes
public:
    RefVkFramebuffer            m_FrameBuffer{};
protected:
    std::string                 m_Name;
    RenderPassClearData         m_RenderPassClearData;
};
