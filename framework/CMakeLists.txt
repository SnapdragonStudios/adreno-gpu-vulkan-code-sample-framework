cmake_minimum_required (VERSION 3.10 FATAL_ERROR)  # 3.7 added FindVulkan, 3.10 supported MSVC c++17 (without manually setting flags)
cmake_policy(VERSION 3.7)

project (framework C CXX)

set(CMAKE_CXX_STANDARD 17)

# Graphics API independant (base functionality) source here
set(BASE_CPP_SRC
    code/system/crc32c.hpp
    code/system/glm_common.hpp
    code/system/math_common.hpp
    code/system/os_common.cpp
    code/system/os_common.h
    code/mesh/instanceGenerator.cpp
    code/mesh/instanceGenerator.hpp
    code/mesh/meshLoader.cpp
    code/mesh/meshLoader.hpp
    code/mesh/meshObjectIntermediate.cpp
    code/mesh/meshObjectIntermediate.hpp
    code/mesh/octree.cpp
    code/mesh/octree.hpp
)

# OS independant (Vulkan targetted) source here
set(VULKAN_CPP_SRC
    code/main/frameworkApplicationBase.cpp
    code/main/frameworkApplicationBase.hpp
    code/main/applicationHelperBase.cpp
    code/main/applicationHelperBase.hpp
    code/memory/bufferObject.cpp
    code/memory/bufferObject.hpp
    code/memory/drawIndirectBufferObject.cpp
    code/memory/drawIndirectBufferObject.hpp
    code/memory/indexBufferObject.cpp
    code/memory/indexBufferObject.hpp
    code/memory/memoryManager.cpp
    code/memory/memoryManager.hpp
    code/memory/memoryMapped.hpp
    code/memory/vertexBufferObject.cpp
    code/memory/vertexBufferObject.hpp
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
    code/gui/gui.hpp
    code/gui/imguiVulkan.cpp
    code/gui/imguiVulkan.hpp
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
    code/helper/postProcess.hpp
    code/helper/postProcessStandard.cpp
    code/helper/postProcessStandard.hpp
    code/helper/postProcessSMAA.cpp
    code/helper/postProcessSMAA.hpp
    code/shadow/shadow.cpp
    code/shadow/shadow.hpp
    code/shadow/shadowVsm.cpp
    code/shadow/shadowVsm.hpp
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
    code/vulkan/vulkan_support.cpp
    code/vulkan/vulkan_support.hpp
)

# Platform Specific source files here
if(WIN32)
    set(BASE_CPP_SRC ${BASE_CPP_SRC}
                code/system/windows/windowsAssetManager.cpp
    )
    set(VULKAN_CPP_SRC ${VULKAN_CPP_SRC}
                code/main/windows/winMain.cpp
                code/gui/windows/imguiWindows.cpp
     )
else()
    set(VULKAN_CPP_SRC ${VULKAN_CPP_SRC}
                code/main/android/androidMain.cpp
                code/memory/androidHardwareBuffer.cpp
                code/memory/androidHardwareBuffer.hpp
                code/gui/android/imguiAndroid.cpp
                code/system/android/androidAssetManager.cpp
    )
    include_directories( ${ANDROID_NDK}/sources/android/native_app_glue/ )
endif()

# Any externals we need to compile as part of framework here
set(EXTERNAL_BASE_SRC
    external/tinyobjloader/tiny_obj_loader.cc
)
set(EXTERNAL_SRC
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui.h
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
    schema/tcb_span.natvis
    external/glm/util/glm.natvis
    external/json/nlohmann_json.natvis
)
set(NATVIS_SCHEMA
    external/VulkanMemoryAllocator/src/vk_mem_alloc.natvis
    external/imgui/misc/debuggers/imgui.natvis
)

# Create the Framework libraries
add_library(frameworkBase STATIC ${BASE_CPP_SRC} ${EXTERNAL_BASE_SRC} ${BASE_NATVIS_SCHEMA})
add_library(framework STATIC ${VULKAN_CPP_SRC} ${EXTERNAL_SRC} ${JSON_SCHEMA} ${NATVIS_SCHEMA})

target_include_directories(frameworkBase PUBLIC code)
target_include_directories(frameworkBase PUBLIC external)
target_include_directories(frameworkBase PUBLIC external/glm)  # so code can do #include "glm/mat3x3.hpp" etc
target_include_directories(frameworkBase PUBLIC external/json/single_include)
target_include_directories(frameworkBase PUBLIC external/span/include)

target_include_directories(framework PUBLIC code)
target_include_directories(framework PUBLIC external)
target_include_directories(framework PUBLIC external/glm)  # so code can do #include "glm/mat3x3.hpp" etc
target_include_directories(framework PUBLIC external/json/single_include)
target_include_directories(framework PUBLIC external/imgui)
target_include_directories(framework PUBLIC external/span/include)

# Vulkan library
find_package(Vulkan REQUIRED)
set_target_properties(Vulkan::Vulkan PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")   # remove the Vulkan incude paths from the local VulkanSDK 
target_include_directories(framework PUBLIC external/Vulkan-Headers/include)        # point the framework to the Vulkan includes that we have as a submodule
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

    target_compile_options(frameworkBase PRIVATE -Wno-nullability-completeness)
    target_compile_definitions(frameworkBase PRIVATE OS_ANDROID)
    target_link_libraries(frameworkBase android log)

    target_compile_options(framework PRIVATE -Wno-nullability-completeness)
    target_compile_definitions(framework PRIVATE OS_ANDROID)
    target_link_libraries(framework frameworkBase)
endif()

if(WIN32)
    target_compile_definitions(framework PRIVATE OS_WINDOWS;_CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(frameworkBase PRIVATE OS_WINDOWS;_CRT_SECURE_NO_WARNINGS)
endif()

# framework links frameworkBase
target_link_libraries(framework frameworkBase)

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.8")
    # create MSVC hierachy (if appropriate)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/code" PREFIX "code" FILES ${VULKAN_CPP_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/code" FILES ${BASE_CPP_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/external" PREFIX "external" FILES ${EXTERNAL_SRC})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/schema" PREFIX "schema" FILES ${JSON_SCHEMA})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "natvis" FILES ${NATVIS_SCHEMA})
endif()
