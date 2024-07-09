cmake_minimum_required (VERSION 3.21 FATAL_ERROR)  # 3.7 added FindVulkan, 3.10 supported MSVC c++17 (without manually setting flags), 3.19 (maybe before) supported C++20 on Win/Android, 3.21 added Android NDK support with NDK r23 although we are not currently using it!
cmake_policy(VERSION 3.7)

project (framework C CXX)

set(CMAKE_CXX_STANDARD 20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

# Platform and Graphics API independant source here
# Graphics API independant (base functionality) source here
set(CPP_BASE_SRC
    code/system/crc32c.hpp
    code/system/glm_common.hpp
    code/system/math_common.hpp
    code/system/os_common.cpp
    code/system/os_common.h
    code/mesh/instanceGenerator.cpp
    code/mesh/instanceGenerator.hpp
    code/mesh/meshLoader.cpp
    code/mesh/meshLoader.hpp
    code/mesh/meshIntermediate.cpp
    code/mesh/meshIntermediate.hpp
    code/mesh/octree.cpp
    code/mesh/octree.hpp
    code/texture/textureManager.cpp
    code/texture/textureManager.hpp
)

set(CPP_GENERIC_SRC
    code/main/frameworkApplicationBase.cpp
    code/main/frameworkApplicationBase.hpp
    code/animation/animation.cpp
    code/animation/animation.hpp
    code/animation/animationData.hpp
    code/animation/animationGltfLoader.cpp
    code/animation/animationGltfLoader.hpp
    code/animation/skeleton.cpp
    code/animation/skeleton.hpp
    code/animation/skeletonData.cpp
    code/animation/skeletonData.hpp
    code/animation/skeletonGltfLoader.cpp
    code/animation/skeletonGltfLoader.hpp
    code/camera/camera.cpp
    code/camera/camera.hpp
    code/camera/cameraController.cpp
    code/camera/cameraController.hpp
    code/camera/cameraControllerAnim.cpp
    code/camera/cameraControllerAnim.hpp
    code/camera/cameraControllerTouch.cpp
    code/camera/cameraControllerTouch.hpp
    code/camera/cameraData.hpp
    code/camera/cameraGltfLoader.cpp
    code/camera/cameraGltfLoader.hpp
    code/graphicsApi/graphicsApiBase.cpp
    code/graphicsApi/graphicsApiBase.hpp
    code/gui/gui.hpp
    code/gui/imguiBase.cpp
    code/gui/imguiBase.hpp
    code/gui/imguiPlatform.hpp
    code/light/light.cpp
    code/light/light.hpp
    code/light/lightData.hpp
    code/light/lightList.cpp
    code/light/lightList.hpp
    code/light/lightListGui.cpp
    code/light/lightListGui.hpp
    code/light/lightGltfLoader.cpp
    code/light/lightGltfLoader.hpp
    code/light/lightLoader.cpp
    code/light/lightLoader.hpp
    code/material/shader.hpp
    code/material/shaderModule.hpp
    code/material/pipelineLayout.hpp
    code/material/pipelineVertexInputState.hpp
    code/material/specializationConstantsLayout.hpp
    code/memory/buffer.hpp
    code/memory/memory.hpp
    code/memory/memoryManager.hpp
    code/memory/memoryMapped.hpp
    code/memory/indexBuffer.hpp
    code/memory/vertexBuffer.hpp
    code/memory/uniform.hpp
    code/mesh/instanceGenerator.cpp
    code/mesh/instanceGenerator.hpp
    code/mesh/mesh.hpp
    code/mesh/meshHelper.cpp
    code/mesh/meshHelper.hpp
    code/shadow/shadow.cpp
    code/shadow/shadow.hpp
    code/system/assetManager.hpp
    code/system/config.cpp
    code/system/config.h
    code/system/containers.cpp
    code/system/containers.h
    code/system/profile.cpp
    code/system/profile.h
    code/system/timer.cpp
    code/system/timer.hpp
    code/system/worker.cpp
    code/system/worker.h
    code/texture/loaderKtx.cpp
    code/texture/loaderKtx.hpp
    code/texture/loaderPpm.cpp
    code/texture/loaderPpm.hpp
    code/texture/texture.hpp
    code/texture/textureFormat.cpp
    code/texture/textureFormat.hpp
)

# OS independant (Vulkan targetted) source here
set(CPP_VULKAN_SRC
    code/main/applicationHelperBase.cpp
    code/main/applicationHelperBase.hpp
    code/gui/imguiVulkan.cpp
    code/gui/imguiVulkan.hpp
    code/helper/postProcess.hpp
    code/helper/postProcessStandard.cpp
    code/helper/postProcessStandard.hpp
    code/helper/postProcessSMAA.cpp
    code/helper/postProcessSMAA.hpp
    code/material/descriptorSetDescription.hpp
    code/material/descriptorSetLayout.cpp
    code/material/descriptorSetLayout.hpp
    code/material/material.cpp
    code/material/material.hpp
    code/material/materialManager.cpp
    code/material/materialManager.hpp
    code/material/materialManagerT.hpp
    code/material/materialProps.cpp
    code/material/materialProps.h
    code/material/materialShaderDefinition.cpp
    code/material/materialShaderDefinition.hpp
    code/material/computable.cpp
    code/material/computable.hpp
    code/material/drawable.cpp
    code/material/drawable.hpp
    code/material/vulkan/pipelineLayout.cpp
    code/material/vulkan/pipelineLayout.hpp
    code/material/vulkan/pipelineVertexInputState.cpp
    code/material/vulkan/pipelineVertexInputState.hpp
    code/material/shaderDescription.cpp
    code/material/shaderDescription.hpp
    code/material/shaderManager.cpp
    code/material/shaderManager.hpp
    code/material/shaderManagerT.hpp
    code/material/vulkan/shader.cpp
    code/material/vulkan/shader.hpp
    code/material/vulkan/shaderModule.cpp
    code/material/vulkan/shaderModule.hpp
    code/material/specializationConstantDescription.hpp
    code/material/vulkan/material.hpp
    code/material/vulkan/specializationConstantsLayout.cpp
    code/material/vulkan/specializationConstantsLayout.hpp
    code/material/vulkan/vertexDescription.cpp
    code/material/vulkan/vertexDescription.hpp
    code/material/vertexFormat.hpp
    code/memory/vulkan/bufferObject.cpp
    code/memory/vulkan/bufferObject.hpp
    code/memory/vulkan/drawIndirectBufferObject.cpp
    code/memory/vulkan/drawIndirectBufferObject.hpp
    code/memory/vulkan/indexBufferObject.cpp
    code/memory/vulkan/indexBufferObject.hpp
    code/memory/vulkan/memoryManager.cpp
    code/memory/vulkan/memoryManager.hpp
    code/memory/vulkan/memoryMapped.hpp
    code/memory/vulkan/uniform.cpp
    code/memory/vulkan/uniform.hpp
    code/memory/vulkan/vertexBufferObject.cpp
    code/memory/vulkan/vertexBufferObject.hpp
    code/shadow/shadow.cpp
    code/shadow/shadow.hpp
    code/shadow/shadowVsm.cpp
    code/shadow/shadowVsm.hpp
    code/shadow/shadowVulkan.cpp
    code/shadow/shadowVulkan.hpp
    code/texture/vulkan/imageWrapper.cpp
    code/texture/vulkan/imageWrapper.hpp
    code/texture/vulkan/loaderKtx.cpp
    code/texture/vulkan/loaderKtx.hpp
    code/texture/vulkan/texture.cpp
    code/texture/vulkan/texture.hpp
    code/texture/vulkan/textureManager.cpp
    code/texture/vulkan/textureManager.hpp
    code/vulkan/commandBuffer.cpp
    code/vulkan/commandBuffer.hpp
    code/vulkan/extension.cpp
    code/vulkan/extension.hpp
    code/vulkan/extensionHelpers.cpp
    code/vulkan/extensionHelpers.hpp
    code/vulkan/MeshObject.cpp
    code/vulkan/MeshObject.h
    code/vulkan/renderTarget.cpp
    code/vulkan/renderTarget.hpp
    code/vulkan/TextureFuncts.cpp
    code/vulkan/TextureFuncts.h
    code/vulkan/vulkan.cpp
    code/vulkan/vulkan.hpp
    code/vulkan/vulkanDebugCallback.cpp
    code/vulkan/vulkanDebugCallback.hpp
    code/vulkan/timerPool.cpp
    code/vulkan/timerPool.hpp
    code/vulkan/timerSimple.cpp
    code/vulkan/timerSimple.hpp
    code/vulkan/vulkan_support.cpp
    code/vulkan/vulkan_support.hpp
)

# Platform Specific source files here
if(WIN32)
    set(CPP_BASE_SRC ${CPP_BASE_SRC}
                code/system/windows/windowsAssetManager.cpp
    )
    set(CPP_GENERIC_SRC ${CPP_GENERIC_SRC}
                code/main/windows/winMain.cpp
                code/gui/windows/imguiWindows.cpp
     )
else()
    set(CPP_GENERIC_SRC ${CPP_GENERIC_SRC}
                code/main/android/androidMain.cpp
                code/memory/androidHardwareBuffer.cpp
                code/memory/androidHardwareBuffer.hpp
                code/gui/android/imguiAndroid.cpp
                code/system/android/androidAssetManager.cpp
    )
    set(CPP_VULKAN_SRC ${CPP_VULKAN_SRC}
    )
    include_directories( ${ANDROID_NDK}/sources/android/native_app_glue/ )
endif()

# Framework support for Ray Tracing/Query
if(TRUE)
    set(CPP_VULKAN_SRC ${CPP_VULKAN_SRC}
                code/vulkanRT/accelerationStructure.cpp
                code/vulkanRT/accelerationStructure.hpp
                code/vulkanRT/accelerationInstanceBufferObject.cpp
                code/vulkanRT/accelerationInstanceBufferObject.hpp
                code/vulkanRT/extensionHelpersRT.hpp
                code/vulkanRT/meshObjectRT.cpp
                code/vulkanRT/meshObjectRT.hpp
                code/vulkanRT/meshUpdateRT.cpp
                code/vulkanRT/meshUpdateRT.hpp
                code/vulkanRT/sceneRT.cpp
                code/vulkanRT/sceneRT.hpp
                code/vulkanRT/traceable.cpp
                code/vulkanRT/traceable.hpp
                code/vulkanRT/vulkanRT.cpp
                code/vulkanRT/vulkanRT.hpp
    )
endif()

# Any externals we need to compile as part of framework here
set(EXTERNAL_BASE_SRC
    external/tinyobjloader/tiny_obj_loader.cc
)
set(EXTERNAL_SRC
    external/imgui/imgui.h
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp

    external/implot/implot.h
    external/implot/implot.cpp
    external/implot/implot_items.cpp
    external/implot/implot_internal.h
    external/implot/implot_demo.cpp
)
set(EXTERNAL_VULKAN_SRC
    external/imgui/backends/imgui_impl_vulkan.cpp
    external/imgui/backends/imgui_impl_vulkan.h
)
# Platform Specific external files to compile here
if(WIN32)
    set(EXTERNAL_SRC ${EXTERNAL_SRC}
                     external/imgui/backends/imgui_impl_win32.cpp
                     external/imgui/backends/imgui_impl_win32.h
    )
else()
    set(EXTERNAL_SRC ${EXTERNAL_SRC}
    )
endif()

# Json schemas here (so they are shown in the Solution Explorer)
set(JSON_SCHEMA
    schema/lightsSchema.json
    schema/shaderSchema.json
)

# Visualizers (Visual Studio)
set(BASE_NATVIS_SCHEMA
    external/glm/util/glm.natvis
    external/json/nlohmann_json.natvis
)
set(NATVIS_SCHEMA
    external/VulkanMemoryAllocator/src/vk_mem_alloc.natvis
    external/imgui/misc/debuggers/imgui.natvis
)

# 'Helper' function to call add_sub_directory() and put everything inside a named folder (rather than it littering the top level folder)
define_property(TARGET PROPERTY FOLDER INHERITED BRIEF_DOCS "Set the folder name." FULL_DOCS  "Use to organize targets in an IDE.")
function(add_subdirectory_with_folder _folder_name _folder)
    add_subdirectory(${_folder} ${ARGN})
    set_property(DIRECTORY "${_folder}" PROPERTY FOLDER "${_folder_name}")
endfunction()

# Add the external KXT-Software project (restrict to ktx_read library)
if(TRUE)
    set(KTX_FEATURE_TOOLS OFF)
    set(KTX_FEATURE_TESTS OFF)
    set(KTX_FEATURE_STATIC_LIBRARY ON)
    set(KTX_FEATURE_WRITE OFF)
    set(KTX_FEATURE_VULKAN ON)
    set(KTX_FEATURE_GL_UPLOAD OFF)
    set(BASISU_SUPPORT_OPENCL OFF)
    #foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
    #    STRING (REGEX REPLACE "/RTC1" "/O2" ${flag_var} "${${flag_var}}")   # Enable /O2 optimization level in debug builds (ktx library only)
    #endforeach(flag_var)
    add_subdirectory_with_folder("external/KTX-Software" external/KTX-Software EXCLUDE_FROM_ALL)
    #foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
    #    STRING (REGEX REPLACE "/O2" "/RTC1" ${flag_var} "${${flag_var}}")   # Disable /O2 optimizetion (in debug)
    #endforeach(flag_var)
endif()

# Create the gfx api agnostic framework libraries
add_library(framework_base   STATIC ${CPP_BASE_SRC} ${EXTERNAL_BASE_SRC} ${BASE_NATVIS_SCHEMA})
add_library(framework        STATIC ${CPP_GENERIC_SRC} ${EXTERNAL_SRC} ${JSON_SCHEMA} ${NATVIS_SCHEMA})

target_include_directories(framework_base   PUBLIC code)
target_include_directories(framework_base   PUBLIC external)
target_include_directories(framework_base   PUBLIC external/glm)  # so code can do #include "glm/mat3x3.hpp" etc
target_include_directories(framework_base   PUBLIC external/json/single_include)

target_include_directories(framework        PUBLIC code)
target_include_directories(framework        PUBLIC external)
target_include_directories(framework        PUBLIC external/glm)  # so code can do #include "glm/mat3x3.hpp" etc
target_include_directories(framework        PUBLIC external/json/single_include)
target_include_directories(framework        PUBLIC external/imgui)
target_include_directories(framework        PUBLIC external/implot)

# Make sure our graphics api flags are set (Android in particular may/will not have gone through a top level CMakeLists.txt to set these)
if(NOT DEFINED FRAMEWORK_ENABLE_VULKAN)
  set(FRAMEWORK_ENABLE_VULKAN ON)
endif()

# Vulkan framework support
if(FRAMEWORK_ENABLE_VULKAN)
  add_library(framework_vulkan STATIC ${CPP_VULKAN_SRC} ${EXTERNAL_VULKAN_SRC})
  target_include_directories(framework_vulkan PUBLIC code)
  target_include_directories(framework_vulkan PUBLIC external)
  target_include_directories(framework_vulkan PUBLIC external/glm)  # so code can do #include "glm/mat3x3.hpp" etc
  target_include_directories(framework_vulkan PUBLIC external/json/single_include)
  target_include_directories(framework_vulkan PUBLIC external/imgui)
  target_include_directories(framework_vulkan PUBLIC external/implot)

  target_link_libraries(framework_vulkan framework)
  find_package(Vulkan REQUIRED)
  set_target_properties(Vulkan::Vulkan PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")   # remove the Vulkan incude paths from the local VulkanSDK 
  target_include_directories(framework_vulkan PUBLIC external/Vulkan-Headers/include)        # point the framework to the Vulkan includes that we have as a submodule

  if (FRAMEWORK_WINDOWS_ARM64)
    target_link_directories(framework_vulkan BEFORE INTERFACE ../project/windows/libs_arm64)
    target_link_libraries(framework_vulkan vulkan-1.lib)
  else()
    target_link_libraries(framework_vulkan Vulkan::Vulkan)
  endif()
endif()

# KTX library
target_link_libraries(framework ktx_read)

if(ANDROID)
    # Setup Android native_app_glue
    #include_directories( ${ANDROID_NDK}/sources/android/native_app_glue/ )
    #add_library( app-glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c )
    #target_link_libraries( framework app-glue )
    message("ndk at ${ANDROID_NDK}")

    # Export ANativeActivity_onCreate(),
    # Refer to: https://github.com/android-ndk/ndk/issues/381.
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u android_main")

    target_link_libraries(framework_base android log)

    target_compile_options(framework_base PRIVATE -Wno-nullability-completeness)
    target_compile_options(framework PRIVATE -Wno-nullability-completeness;-Wno-deprecated-volatile;-Wno-deprecated-anon-enum-enum-conversion)
    target_compile_options(framework_vulkan PRIVATE -Wno-nullability-completeness)

    target_compile_definitions(framework_base PRIVATE OS_ANDROID)
    target_compile_definitions(framework PRIVATE OS_ANDROID)
    target_compile_definitions(framework_vulkan PRIVATE OS_ANDROID)
endif()

if(WIN32)
    target_compile_definitions(framework_base   PRIVATE OS_WINDOWS;_CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(framework        PRIVATE OS_WINDOWS;_CRT_SECURE_NO_WARNINGS)
    if (FRAMEWORK_ENABLE_VULKAN)
      target_compile_definitions(framework_vulkan PRIVATE OS_WINDOWS;_CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# framework links framework_base
target_link_libraries(framework framework_base)

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.8")
    # create MSVC hierachy (if appropriate)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/code" PREFIX "code" FILES ${CPP_VULKAN_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/code" FILES ${CPP_GENERIC_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/code" FILES ${CPP_BASE_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/external" PREFIX "external" FILES ${EXTERNAL_BASE_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/external" PREFIX "external" FILES ${EXTERNAL_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/external" PREFIX "external" FILES ${EXTERNAL_VULKAN_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/schema" PREFIX "schema" FILES ${JSON_SCHEMA})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "natvis" FILES ${NATVIS_SCHEMA})
endif()
