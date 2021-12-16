cmake_minimum_required (VERSION 3.10 FATAL_ERROR)  # 3.7 added FindVulkan, 3.10 supported MSVC c++17 (without manually setting flags)
cmake_policy(VERSION 3.7)

project (framework C CXX)

set(CMAKE_CXX_STANDARD 17)

# Platform independant source here
set(CPP_SRC
    code/main/frameworkApplicationBase.cpp
    code/main/frameworkApplicationBase.hpp
    code/main/applicationHelperBase.cpp
    code/main/applicationHelperBase.hpp
    code/memory/bufferObject.cpp
    code/memory/bufferObject.hpp
    code/memory/indexBufferObject.cpp
    code/memory/indexBufferObject.hpp
    code/memory/memoryManager.cpp
    code/memory/memoryManager.hpp
    code/memory/memoryMapped.hpp
    code/memory/vertexBufferObject.cpp
    code/memory/vertexBufferObject.hpp
    code/system/assetManager.hpp
    code/system/crc32c.hpp
    code/system/glm_common.hpp
    code/system/math_common.hpp
    code/system/os_common.cpp
    code/system/os_common.h
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
    code/camera/camera.cpp
    code/camera/camera.hpp
    code/camera/cameraController.cpp
    code/camera/cameraController.hpp
    code/camera/cameraControllerTouch.cpp
    code/camera/cameraControllerTouch.hpp
    code/gui/gui.hpp
    code/gui/imguiVulkan.cpp
    code/gui/imguiVulkan.hpp
    code/gui/imguiBase.cpp
    code/gui/imguiBase.hpp
    code/gui/imguiPlatform.hpp
    code/material/descriptorSetDescription.hpp
    code/material/descriptorSetLayout.cpp
    code/material/descriptorSetLayout.hpp
    code/material/material.cpp
    code/material/material.hpp
    code/material/materialManager.cpp
    code/material/materialManager.hpp
    code/material/materialShaderDefinition.cpp
    code/material/materialShaderDefinition.hpp
    code/material/computable.cpp
    code/material/computable.hpp
    code/material/drawable.cpp
    code/material/drawable.hpp
    code/material/pipelineLayout.cpp
    code/material/pipelineLayout.hpp
    code/material/pipelineVertexInputState.cpp
    code/material/pipelineVertexInputState.hpp
    code/material/shaderDescription.cpp
    code/material/shaderDescription.hpp
    code/material/shaderManager.cpp
    code/material/shader.cpp
    code/material/shader.hpp
    code/material/shaderManager.hpp
    code/material/shaderModule.cpp
    code/material/shaderModule.hpp
    code/material/vertexDescription.cpp
    code/material/vertexDescription.hpp
    code/material/vertexFormat.hpp
    code/mesh/instanceGenerator.cpp
    code/mesh/instanceGenerator.hpp
    code/mesh/octree.cpp
    code/mesh/octree.hpp
    code/shadow/shadow.cpp
    code/shadow/shadow.hpp
    code/vulkan/MeshObject.cpp
    code/vulkan/MeshObject.h
    code/vulkan/TextureFuncts.cpp
    code/vulkan/TextureFuncts.h
    code/vulkan/vulkan.cpp
    code/vulkan/vulkan.hpp
    code/vulkan/vulkan_support.cpp
    code/vulkan/vulkan_support.hpp
    code/vulkan/VK_QCOM_render_pass_transform.h
)

# Platform Specific source files here
if(WIN32)
    set(CPP_SRC ${CPP_SRC}
                code/main/windows/winMain.cpp
                code/gui/windows/imguiWindows.cpp
                code/system/windows/windowsAssetManager.cpp
    )
else()
    set(CPP_SRC ${CPP_SRC}
                code/main/android/androidMain.cpp
                code/memory/androidHardwareBuffer.cpp
                code/memory/androidHardwareBuffer.hpp
                code/gui/android/imguiAndroid.cpp
                code/system/android/androidAssetManager.cpp
    )
    include_directories( ${ANDROID_NDK}/sources/android/native_app_glue/ )
endif()

# Any externals we need to compile as part of framework here
set(EXTERNAL_SRC
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui.h
    external/imgui/examples/imgui_impl_vulkan.cpp
    external/imgui/examples/imgui_impl_vulkan.h
    external/tinyobjloader/tiny_obj_loader.cc
)
# Platform Specific external files to compile here
if(WIN32)
    set(EXTERNAL_SRC ${EXTERNAL_SRC}
                     external/imgui/examples/imgui_impl_win32.cpp
                     external/imgui/examples/imgui_impl_win32.h
    )
else()
    set(EXTERNAL_SRC ${EXTERNAL_SRC}
    )
endif()


# Create the Framework library
add_library(framework STATIC ${CPP_SRC} ${EXTERNAL_SRC})

target_include_directories(framework PUBLIC code)
target_include_directories(framework PRIVATE Vulkan::Vulkan)
target_include_directories(framework PUBLIC external)
target_include_directories(framework PUBLIC external/glm)  # so code can do #include "glm/mat3x3.hpp" etc
target_include_directories(framework PUBLIC external/json/single_include)
target_include_directories(framework PUBLIC external/imgui)
target_include_directories(framework PUBLIC external/span/include)

# Vulkan library
find_package(Vulkan REQUIRED)
target_link_libraries(framework Vulkan::Vulkan)

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
    target_compile_options(framework PRIVATE -Wno-nullability-completeness)
    target_compile_definitions(framework PRIVATE OS_ANDROID)
    target_link_libraries(framework 
                          android 
                          log)
endif()

if(WIN32)
    target_compile_definitions(framework PRIVATE OS_WINDOWS;_CRT_SECURE_NO_WARNINGS)
endif()

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.8")
    # create MSVC hierachy (if appropriate)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/code" FILES ${CPP_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/external" PREFIX "external" FILES ${EXTERNAL_SRC})
endif()
