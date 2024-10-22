cmake_minimum_required (VERSION 3.21)

project (sgsr2 C CXX)
set(CMAKE_CXX_STANDARD 20)

#
# Source files included in this application.
#

set(CPP_SRC code/main/application.cpp
            code/main/application.hpp
            code/main/sgsr2_context.cpp
            code/main/sgsr2_context.hpp
            code/main/sgsr2_context_frag.cpp
            code/main/sgsr2_context_frag.hpp
)

#
# Setup the module path to include the 'project directory' (project/windows or project/android)
#
if(NOT DEFINED PROJECT_ROOT_DIR)
    set(PROJECT_ROOT_DIR ${CMAKE_SOURCE_DIR})   # Windows can use CMAKE_SOURCE_DIR, Android needs build.gradle needs "-DPROJECT_ROOT_DIR=${project.rootDir}" in call to cmake set since there is not a 'top' cmakefile (gradle is top level)
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_ROOT_DIR}/cmake)

#
# Add in the contents of 'shaders' directory
#
include(AddShadersDir)

#
# Do all the build steps for a Framework application.
# needs Framework_dir and project_name variables.
#
include(FrameworkApplicationHelper)

#
# Copy required models to local folders
#
include(ModelPackager)

# Museum GLTF
add_gltf(${CMAKE_CURRENT_SOURCE_DIR}/../../assets/scenes/Museum/Museum)

#
# Convert and copy textures to local folders
#
include(TexturePackager)

# Textures
add_textures_from_path(${CMAKE_CURRENT_SOURCE_DIR}/../../assets/scenes/Museum/Textures)
add_textures_from_path(${CMAKE_CURRENT_SOURCE_DIR}/../../assets/textures)