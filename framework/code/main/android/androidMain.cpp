// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/// @file androidMain.cpp
/// @brief Android 'android_main' application entry point and event handler.
/// 
/// Implements Android specific wrapping of frameworkApplicationBase.
//  There should be the minimum (possible) amount of code in here.

#include <android_native_app_glue.h>
#include "main/frameworkApplicationBase.hpp"
#include "main/applicationEntrypoint.hpp"
#include <cassert>
#include <string>
#include "system/os_common.h"
#include "vulkan/vulkan.hpp"
#include "gui/gui.hpp"


///////////////////////////////////////////////////////////////////////////////
// Begin Android Glue entry point

// Shared state for our app.
struct Engine
{
private:
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
public:
    Engine(struct android_app* _app, FrameworkApplicationBase* _sample) : app(_app), application(_sample) {}
    struct android_app* const app = nullptr;
    FrameworkApplicationBase* const application = nullptr;
    bool animating = false;
    bool initialized = false;
    bool guiTouchActive = false;
};

//-----------------------------------------------------------------------------
static bool InitializeVulkan( FrameworkApplicationBase &rApplication )
//-----------------------------------------------------------------------------
{
    // Initialize some Vulkan stuff
    // Do this BEFORE calling CreateVulkanWindow()
    return true;
}

//-----------------------------------------------------------------------------
// Process the next main command.
static void engine_handle_cmd(struct android_app* app, int32_t cmd)
//-----------------------------------------------------------------------------
{
    struct Engine* engine = (struct Engine*)app->userData;

    // These commands are defined in "android_native_app_glue.h"
    switch (cmd)
    {
        case APP_CMD_INPUT_CHANGED:
            LOGE("Processing APP_CMD_INPUT_CHANGED");
            break;
        case APP_CMD_INIT_WINDOW:
        {
            LOGE("Processing APP_CMD_INIT_WINDOW");
            assert(!engine->initialized);

            // InitializeVulkan
            const int iDesiredMSAA = 4;   // 0 = off, -1 = best available, [2|4|8|16] = specified
            assert(engine->application);
            assert(engine->application->GetVulkan());
            assert(engine->app->window);

            if (!engine->application->GetVulkan()->Init( (uintptr_t)(void*)engine->app->window,
                                                   0,
                                                   iDesiredMSAA,
                                                   [engine](tcb::span<const VkSurfaceFormatKHR> x) { return engine->application->PreInitializeSelectSurfaceFormat(x); },
                                                   [engine](Vulkan::AppConfiguration& x) { return engine->application->PreInitializeSetVulkanConfiguration(x); }))
            {
                LOGE("Unable to initialize Vulkan!!");
                engine->initialized = false;    //assert above already checked this, but set anyhow
            }
            else if(!engine->application->Initialize( (uintptr_t)(void*)engine->app->window) )
            {
                LOGE("VkSample::Initialize Error");
                engine->initialized = false;    //assert above already checked this, but set anyhow
            }
            else
            {
                LOGI("VkSample::Initialize Success");
                engine->initialized = true;
            }
            engine->animating = true;
            break;
        }
        case APP_CMD_TERM_WINDOW:
            LOGE("Processing APP_CMD_TERM_WINDOW");
            engine->application->Destroy();
            engine->initialized = false;
            break;
        case APP_CMD_WINDOW_RESIZED:
            LOGE("Processing APP_CMD_WINDOW_RESIZED");
            break;
        case APP_CMD_WINDOW_REDRAW_NEEDED:
            LOGE("Processing APP_CMD_WINDOW_REDRAW_NEEDED");
            break;
        case APP_CMD_CONTENT_RECT_CHANGED:
            LOGE("Processing APP_CMD_CONTENT_RECT_CHANGED");
            break;
        case APP_CMD_GAINED_FOCUS:
            LOGE("Processing APP_CMD_GAINED_FOCUS");
            engine->animating = true;
            break;
        case APP_CMD_LOST_FOCUS:
            LOGE("Processing APP_CMD_LOST_FOCUS");
            engine->animating = false;
            break;
        case APP_CMD_CONFIG_CHANGED:
            LOGE("Processing APP_CMD_CONFIG_CHANGED");
            break;
        case APP_CMD_LOW_MEMORY:
            LOGE("Processing APP_CMD_LOW_MEMORY");
            break;
        case APP_CMD_START:
            LOGE("Processing APP_CMD_START");
            break;
        case APP_CMD_RESUME:
            LOGE("Processing APP_CMD_RESUME");
            break;
        case APP_CMD_SAVE_STATE:
            LOGE("Processing APP_CMD_SAVE_STATE");
            // Teardown, and recreate each time
            engine->animating = false;
            engine->application->Destroy();
            engine->initialized = false;
            break;
        case APP_CMD_PAUSE:
            LOGE("Processing APP_CMD_PAUSE");
            break;
        case APP_CMD_STOP:
            LOGE("Processing APP_CMD_STOP");
            break;
        case APP_CMD_DESTROY:
            LOGE("Processing APP_CMD_DESTROY");
            break;

        default:
            LOGE("Unknowns APP_CMD_XXXXXX = %d", cmd);
            break;
    }
}

//-----------------------------------------------------------------------------
int32_t Input_CBHandler(struct android_app *pAndroidApp, AInputEvent* event)
//-----------------------------------------------------------------------------
{
    // Grab the engine from User Data
    struct Engine* engine = (struct Engine*)pAndroidApp->userData;

    //*****************************************************
    // Touch Events
    //*****************************************************
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
    {
        int iPointerAction = AMotionEvent_getAction(event);
        int iPointerIndex = (iPointerAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int iPointerID = AMotionEvent_getPointerId(event, iPointerIndex);
        int iAction = (iPointerAction & AMOTION_EVENT_ACTION_MASK);

        int iHistorySize = 0;
        int iPointerCount = 0;

        float fltOneX = AMotionEvent_getX(event, iPointerIndex);
        float fltOneY = AMotionEvent_getY(event, iPointerIndex);

        switch (iAction)
        {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            if (iPointerID == 0 && engine->application->GetGui())
            {
                engine->guiTouchActive = engine->application->GetGui()->TouchDownEvent(iPointerID, fltOneX, fltOneY);
            }
            if ((iPointerID != 0) || !engine->guiTouchActive)
            {
                engine->application->TouchDownEvent(iPointerID, fltOneX, fltOneY);
            }
            break;

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            if ((iPointerID == 0) && engine->application->GetGui())
            {
                engine->application->GetGui()->TouchUpEvent(iPointerID, fltOneX, fltOneY);
            }
            if ((iPointerID != 0) || !engine->guiTouchActive)
            {
                // Send the application the 'Touch up' event
                engine->application->TouchUpEvent(iPointerID, fltOneX, fltOneY);
            }
            engine->guiTouchActive = false;
            break;

        case AMOTION_EVENT_ACTION_MOVE:
            iHistorySize = AMotionEvent_getHistorySize(event);
            iPointerCount = AMotionEvent_getPointerCount(event);
            for (int iHistoryIndx = 0; iHistoryIndx < iHistorySize; iHistoryIndx++)
            {
                for (int iPointerIndx = 0; iPointerIndx < iPointerCount; iPointerIndx++)
                {
                    iPointerID = AMotionEvent_getPointerId(event, iPointerIndx);
                    fltOneX = AMotionEvent_getHistoricalX(event, iPointerIndx, iHistoryIndx);
                    fltOneY = AMotionEvent_getHistoricalY(event, iPointerIndx, iHistoryIndx);
                }
            }

            for (int iPointerIndx = 0; iPointerIndx < iPointerCount; iPointerIndx++)
            {
                iPointerID = AMotionEvent_getPointerId(event, iPointerIndx);
                fltOneX = AMotionEvent_getX(event, iPointerIndx);
                fltOneY = AMotionEvent_getY(event, iPointerIndx);
                if ((iPointerID == 0) && engine->application->GetGui())
                {
                    bool prevGuiTouchActive = engine->guiTouchActive;
                    engine->guiTouchActive = engine->application->GetGui()->TouchMoveEvent(iPointerID, fltOneX, fltOneY);
                    if (prevGuiTouchActive && !engine->guiTouchActive)
                    {
                        // the gui is now consuming events, rather than creating a discontinuity just tell the application finger is 'up'
                        engine->application->TouchUpEvent(iPointerID, fltOneX, fltOneY);
                    }
                }
                if ((iPointerID != 0) || !engine->guiTouchActive)
                {
                    engine->application->TouchMoveEvent(iPointerID, fltOneX, fltOneY);
                }
            }
            break;

        default:
            //LOGI("Unhandled Touch Event: 0x%X", iPointerAction);
            break;

        }

    }   // Touch Event

    // Return (bool) 0|1 if handled here or not
    return 0;
}


/// This is the main entry point of a native application that is using
/// android_native_app_glue.  It runs in its own thread, with its own
/// event loop for receiving input events and doing other things.
//-----------------------------------------------------------------------------
void android_main(struct android_app* state)
//-----------------------------------------------------------------------------
{
    struct Engine engine( state, Application_ConstructApplication() );
    assert(engine.application);
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = Input_CBHandler;

    //
    // Get the application name.
    ANativeActivity* activity = state->activity;
    JNIEnv* env = 0;
    activity->vm->AttachCurrentThread(&env, NULL);

    jclass clazz = env->GetObjectClass(activity->clazz);
    jmethodID methodID = env->GetMethodID(clazz, "getPackageName", "()Ljava/lang/String;");
    jobject result = env->CallObjectMethod(activity->clazz, methodID);

    jboolean isCopy;
    std::string res = env->GetStringUTFChars((jstring)result, &isCopy);
    LOGI("Looked up package name: %s", res.c_str());

    OS_SetApplicationName(res.c_str());

    activity->vm->DetachCurrentThread();


    // Give the assetManager instance to the application so we can load assets from the apk file.
    engine.application->SetAndroidAssetManager(state->activity->assetManager);

    // Load the config file.
    engine.application->LoadConfigFile();

    // loop waiting for stuff to do.
    while (1)
    {
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident=ALooper_pollAll(engine.animating ? 0 : -1, nullptr, &events,
                                      (void**)&source)) >= 0)
        {
            // Process this event.
            if (source != nullptr)
            {
                source->process(state, source);
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0)
            {
                LOGI("Exiting");
                engine.application->Destroy();
                delete engine.application;
                return;
            }
        }

        if (engine.animating && engine.initialized)
        {
            engine.application->Render();
        }
    }
}
//END Android Glue
///////////////////////////////////////////////////////////////////////////////
