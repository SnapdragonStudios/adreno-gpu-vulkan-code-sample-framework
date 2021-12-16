// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <memory>
#include "system/assetManager.hpp"
#include "vulkan/TextureFuncts.h"
#include "vulkan/MeshObject.h"
#include "system/config.h"
#include "tcb/span.hpp"
// Material Descriptions
#include "material/materialProps.h" 

// Forward declares
class Vulkan;
class Gui;

//#########################################################
// Config options - Begin
//#########################################################
// ************************************
// General Settings
// ************************************
EXTERN_VAR(uint32_t, gSurfaceWidth);
EXTERN_VAR(uint32_t, gSurfaceHeight);

EXTERN_VAR(uint32_t, gRenderWidth);
EXTERN_VAR(uint32_t, gRenderHeight);

EXTERN_VAR(uint32_t, gReflectMapWidth);
EXTERN_VAR(uint32_t, gReflectMapHeight);

EXTERN_VAR(uint32_t, gShadowMapWidth);
EXTERN_VAR(uint32_t, gShadowMapHeight);

EXTERN_VAR(uint32_t, gHudRenderWidth);
EXTERN_VAR(uint32_t, gHudRenderHeight);

//#########################################################
// Config options - End
//#########################################################

/// Base class from which Applications must derive.
/// Application should implement a function "Application_ConstructApplication" that returns a new pointer to their main implemention class that derives from this class.
class FrameworkApplicationBase
{
    FrameworkApplicationBase& operator=(const FrameworkApplicationBase&) = delete;
    FrameworkApplicationBase(const FrameworkApplicationBase&) = delete;

public:
    FrameworkApplicationBase();
    virtual ~FrameworkApplicationBase();

    /// Set the AAssetManager (on android, does nothing on windows).  Do before LoadConfigFile().
    void SetAndroidAssetManager(AAssetManager* pAAssetManager);

    /// Load config file before Initialize()
    /// This will go to derived class that can then set global parameters
    virtual bool    LoadConfigFile();

    /// May be called before initialize (from Vulkan.cpp, during inital Vulkan setup).
    /// @return index of the VkSurfaceFormatKHR you want to use, or -1 for default.
    virtual int     PreInitializeSelectSurfaceFormat(tcb::span<const VkSurfaceFormatKHR>);

    /// May be called before initialize (from Vulkan.cpp, during inital Vulkan setup).
    /// @param configuration (if untouched, Vulkan will be setup using defaults).
    virtual void    PreInitializeSetVulkanConfiguration( Vulkan::AppConfiguration& );

    /// Initialize application.
    /// This is the main entry point for your Application code!
    virtual bool    Initialize(uintptr_t windowHandle);

    /// Destroy application (opposite of Initialize, must get back to state that is re-Initializable)
    virtual void    Destroy();

    /// Called by framework whenever the screen (swap chain) size has changed.
    virtual bool    SetSize(uint32_t width, uint32_t height);

    /// Application Main thread 'render' loop (eg ALooper_pollAll loop on Android)
    /// Called every frame.
    /// Application should derive from this and issue their renderring commands from this function.
    virtual void    Render(float frameTimeSeconds) = 0;

    /// Keyboard Input 'down' event call (application should override to recieve these events)
    virtual void    KeyDownEvent(uint32_t key);
    /// Keyboard Input 'repeat' event call (application should override to recieve these events)
    virtual void    KeyRepeatEvent(uint32_t key);
    /// Keyboard Input 'up' event call (application should override to recieve these events)
    virtual void    KeyUpEvent(uint32_t key);
    /// Touch (or mouse) input 'down' event call (application should override to recieve these events)
    virtual void    TouchDownEvent(int iPointerID, float xPos, float yPos);
    /// Touch (or mouse) input 'move' event call (application should override to recieve these events)
    virtual void    TouchMoveEvent(int iPointerID, float xPos, float yPos);
    /// Touch (or mouse) input 'up' event call (application should override to recieve these events)
    virtual void    TouchUpEvent(int iPointerID, float xPos, float yPos);

    // Helper Functions
    void    InitOneLayout(MaterialProps* pMaterial);
    bool    InitOnePipeline(MaterialProps* pMaterial, MeshObject* pMesh, uint32_t TargetWidth, uint32_t TargetHeight, VkRenderPass RenderPass);
    bool    InitDescriptorPool(MaterialProps* pMaterial);
    bool    InitDescriptorSet(MaterialProps* pMaterial);
    void    SetOneImageLayout(VkImage WhichImage,
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    /// Main thread 'render' loop (eg ALooper_pollAll loop on Android)
    /// Wraps 'Render(float frameTimeSeconds)' with frame timers.  Applications should derive from Render(float frameTimeSeconds).
    void            Render();

    // Accessors
    Vulkan*         GetVulkan() const { return m_vulkan.get(); }
    Gui*            GetGui() const { return m_Gui.get(); }

public:
    // Frame timings
    bool                    m_LogFPS = true;
    float                   m_CurrentFPS;
    float                   m_FpsEvaluateInterval = 0.5f;
    uint32_t                m_LastFpsLogTime;

protected:
    std::unique_ptr<Vulkan> m_vulkan;
    std::unique_ptr<Gui>    m_Gui;
    std::unique_ptr<AssetManager> m_AssetManager;

private:
    uint32_t    m_LastFpsCalcTime;
    uint32_t    m_FpsFrameCount;
    uint32_t    m_LastUpdateTime;
};
