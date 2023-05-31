# Samples

Unless noted all samples run on Windows and Android.

Each sample might have it's own assets dependencies or build instructions, you can find this information on the sample subfolder.

## [empty](empty)

Empty app. Minimal app linked against Framework.

## [hello-gltf](hello-gltf)

Scene (gltf) loading app. Implements a working scene with camera movement and minimal lightning.
This sample demonstrates the most basic usage of the Framework to produce a native Vulkan application and it is designed to be small and simple and meant as a starting point for developers to expand its functionality.

## [hdrSwapchain](hdrSwapchain)

HDRSwapchain demonstrates the use of different swapchain image formats and colorspaces. Has a gui dropdown that allows for switching buffer formats on the fly.
Also demonstrates Qualcomm Vulkan render-pass transform extension VK_QCOM_render_pass_transform.

## [rotatedCopy](rotatedCopy)

Sample to initialize and use the 'VK_QCOM_rotated_copy_commands' Vulkan extension.
Extension may/will need enabling on older Qualcomm Vulkan drivers. Sample does nothing useful on non Qualcomm hardware.

## [shaderResolveTonemap](shaderResolveTonemap)

ShaderResolveTonemap Uses VK_QCOM_render_pass_shader_resolve to perform a filmic tonemapping operator (on a simple forward rendered scene) as part of the MSAA resolve.
Optionally runs the tonemap/resolve as a subpass of the main scene pass.  Has onscreen UI controls to modify MSAA sample counts and to enable/disable the shader resolve and use of subpasses (for measuring GPU subpass/shader-resolve efficiency).

## [SubPass](SubPass)
SubPass sample demos the use of vulkan subpasses to perform a filmic tonemapping operator (on a simple forward rendered scene) and the impact on bandwidth and performance with subpass.

## [BloomImageProcessing](BloomImageProcessing)

This sample demonstrates how to use the VK_QCOM_Image_Processing extension in a simple bloom shader.
The extension provides support for high order filtering and general advanced image processing, features in high demand as screenn sizes get larger and more and more post-processing techniques are developed. For more information, please visit https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_QCOM_image_processing.html
