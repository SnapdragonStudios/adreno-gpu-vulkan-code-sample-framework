
cmake_minimum_required (VERSION 3.21)

project (cooperative_matrix C CXX)
set(CMAKE_CXX_STANDARD 20)

#
# Source files included in this application.
#
set(CPP_SRC
    code/main/application.cpp
    code/main/application.hpp
    code/main/cooperative_matrix_tester.cpp
    code/main/cooperative_matrix_tester.hpp
    code/main/runtime_shader.cpp
    code/main/runtime_shader.hpp
)

#
# Setup the module path to include the 'project directory' (project/windows or project/android)
#
if(NOT DEFINED PROJECT_ROOT_DIR)
    # Windows can use CMAKE_SOURCE_DIR; Android's gradle passes -DPROJECT_ROOT_DIR=${project.rootDir}
    set(PROJECT_ROOT_DIR ${CMAKE_SOURCE_DIR})
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_ROOT_DIR}/cmake)

#
# Do all the build steps for a Framework application.
# (Defines ${TARGET_NAME} inside the helper; do NOT modify the helper that links .a files.)
#
include(FrameworkApplicationHelper)

# ------------------------------------------------------------------------------
# Half (float16) dependency under ../../framework/external
# Build as a normal CMake target and link it to ${TARGET_NAME}.
# ------------------------------------------------------------------------------

add_library(half STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/external/half/half.cpp
)

# Expose headers to dependents (your app target will inherit this include path)
target_include_directories(half
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/external
)

# Nicer grouping in IDEs
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../../framework/external" PREFIX "external" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/external/half/half.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/external/half/half.h
)

# Link half into the application/library target defined by the helper
target_link_libraries(${TARGET_NAME} half)

#
# Setup asset source and target folders
#

# cmake will use our GameSampleAssets (default for no parameter) as root directory for any asset request
# (see FrameworkApplicationHelper.cmake for more info)
inject_root_asset_path()

# Register local variables for asset request, while also defining them in the C++ code for easy access
# Here we use the default destination paths, all defined at FrameworkApplicationHelper.cmake
register_local_asset_path(SHADER_DESTINATION  "${DEFAULT_LOCAL_SHADER_DESTINATION}")
register_local_asset_path(MESH_DESTINATION    "${DEFAULT_LOCAL_MESH_DESTINATION}")
register_local_asset_path(TEXTURE_DESTINATION "${DEFAULT_LOCAL_TEXTURE_DESTINATION}")

#
# Add in the contents of 'shaders' directory
#
include(AddShadersDir)

# Search and include all project shaders
scan_for_shaders()

#
# Copy required models to local folders
#
include(ModelPackager)

# Scene GLTF
add_gltf(scenes/SteamPunkSauna/SteamPunkSauna.gltf)

#
# Convert and copy textures to local folders
#
include(TexturePackager)

# Scene Textures
add_textures_from_path(scenes/SteamPunkSauna UASTC)

# Supporting Textures
add_textures_from_path(textures)
