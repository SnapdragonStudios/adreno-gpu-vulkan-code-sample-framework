# Samples

Unless noted all samples run on Windows and Android.

## [empty](empty)

Empty app.  Minimal app linked against Framework. 

## [hello-gltf](hello-gltf)

Scene (gltf) loading app. Implements a working scene with camera movement and minimal lightning.

## [AODemo](AODemo)

Vulkan implementation of Neural Network Ambient Occlusion.

## [FrameworkTest](FrameworkTest)

Simple test project that initializes the Vulkan Framework and displays a textured sphere.

## [MLClothApp](MLClothApp)

Sample project using machine learning to lower cloth simulation cost.

## [deferredLpac](deferredLpac)

App that renders a (reasonably) complex scene using forward rendering and compute shaders.

Where LPAC (Low Priority Asyncronous Compute) is available the Compute jobs will be done on a low priority queue during shadow pass z-buffer write.

## [DspOffload](dspOffload)

App illustrating how the Hexagon DSP can be used to run graphics tasks and write results to GPU accessable Android Hardware Buffers.

## [forward](forward)

App illustrating a resonably complex forward rendered scene.

## [hdrSwapchain](hdrSwapchain)

Demonstrates the use of different swapchain image formats and colorspaces.  Has a gui dropdown that allows for switching buffer formats on the fly.

Also demonstrates Qualcomm Vulkan render-pass transform extension [VK_QCOM_render_pass_transform](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_QCOM_render_pass_transform.html)

## [rayQueryShadows](rayQueryShadows)

Uses Vulkan Ray Tracing extension (VK_KHR_ray_tracing) to implement shadows using Ray Queries.

Currently Windows only.

## [rotatedCopy](rotatedCopy)

Uses VK_QCOM_rotated_copy_commands (and VK_KHR_copy_commands2) extension to blit from a (lower resolution) intermediate render target to the device framebuffer rotated to match the devices native orientation (and thus avoiding the Android SurfaceFlinger doing an additional rotation/composition step).

## [shaderResolve](shaderResolve)

Uses VK_QCOM_render_pass_shader_resolve extension to implement MSAA and order-independent transparency in a deferred renderer.

## [shaderResolveTonemap](shaderResolveTonemap)

Uses VK_QCOM_render_pass_shader_resolve to perform a filmic tonemapping operator (on a simple forward rendered scene) as part of the MSAA resolve.

Optionally runs the tonemap/resolve as a subpass of the main scene pass.  Has onscreen UI controls to modify MSAA sample counts and to enable/disable the shader resolve and use of subpasses (for measuring GPU subpass/shader-resolve efficiency).

## [atmospherics](atmospherics)

Atmospheric lighting.

# Configuration

Each sample can be configured by adding an 'app_config.txt' file in the root of the relevant sample (ie samples/forward/app_config.txt).

On Android the app_config.txt needs to be pushed to device, into /sdcard/Android/data/ANDROID_APP_ID/files/. , many samples have a batch file to do this (eg 07_InstallConfig.bat).

If this file is missing or empty the sample application should run with 'reasonable' defaults.

Samples share a set of common settings and can define additional settings specific to the sample's functionality.

## Common config settings

gFramesToRender = x

Render a specific number of frames before exiting the app.  x should be in integer.  0 (default) will render 'forever'.

# File handling

## Windows

Executables are compiled to project\windows\solution\samples\APPLICATION\Debug\APPLICATION.exe

Executables should be run from the samples\APPLICATION folder and data files (textures, models, shaders) are loaded from the Media subfolder.  The Visual Studio solution is pre-configured to run the exe from the correct folder.

## Android

Apk application bundles are complied to build\android\APPLICATION\outputs\apk\debug\APPLICATION-debug.apk

So long as the sample's Media files were prepared (02_PrepareMedia.bat) before building the apk, the apk is stand-alone and contains the application executable and Media files.

If desired any files in the Media folder can be 'overridden' by copying the relevant file to /sdcard/Android/data/ANDROID_APP_ID/files/. with the expected folder path.  Eg you can copy a shader file from Media\Shaders\. to /sdcard/Android/data/ANDROID_APP_ID/files/Media/Shaders/. and see your new shader code when the application is re-launched.

