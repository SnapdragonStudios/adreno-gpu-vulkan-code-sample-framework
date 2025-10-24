/*
============================================================================================================
* TILE SHADING SAMPLE
============================================================================================================
*
* This sample demonstrates a light clustering algorithm using Vulkan, with specific support for the
* VK_QCOM_tile_shading extension. This extension allows the application to leverage tile-based deferred rendering
* (TBDR) for efficient memory management and rendering performance.
*
* Reference points:
*
* 1. PreInitializeSetVulkanConfiguration
*    - Sets up the Vulkan configuration, including required and optional extensions.
*    - config.OptionalExtension<ExtensionHelper::Ext_VK_QCOM_tile_shading>();
*    - The application checks for the VK_QCOM_tile_shading extension and sets up the Vulkan configuration accordingly.
*    - If supported, tile shading is enabled at startup.
* 
* 2. Initialize
*    - Initializes various components of the application, including camera, lights, shaders, buffers, render targets, GUI, and mesh objects.
*    - m_IsTileShadingSupported &= GetVulkan()->HasLoadedVulkanDeviceExtension(VK_QCOM_TILE_SHADING_EXTENSION_NAME);
*    - m_IsTileShadingActive = m_IsTileShadingSupported;
* 
* 3. CreateComputables
*    - Creates computable objects for light culling, including tile shading variants.
*    - m_TileShadingPassData.passComputable->DispatchPass(...);
* 
* 4. Render
*    - The main rendering function binds the tile shading extension to the command buffer if tile shading is enabled.
*    - The rendering process includes multiple passes:
*      - Scene Pass: Outputs albedo, normal, and depth.
*      - Light Culling Pass: Divides point lights into clusters and performs light culling.
*      - Deferred Light Pass: Applies the corresponding pixel light cluster to a screen quad.
*    - vkCmdBeginPerTileExecutionQCOM and vkCmdEndPerTileExecutionQCOM are used for the compute dispatch.
*
* SPEC: https://registry.khronos.org/vulkan/specs/latest/man/html/VK_QCOM_tile_shading.html
============================================================================================================
*/