cmake_minimum_required (VERSION 3.21)

project (BloomImageProcessing C CXX)
set(CMAKE_CXX_STANDARD 20)

#
# Source files included in this application.
#

set(CPP_SRC code/main/bloom-image-processing.cpp
            code/main/bloom-image-processing.hpp
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
# Don't include shaders until extensions are part of default GLSLang
#include(AddShadersDir)

#
# Do all the build steps for a Framework application.
# needs Framework_dir and project_name variables.
#
include(FrameworkApplicationHelper)

#
# Copy required assets to local folders
#
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/textures/painting.ktx DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/textures/painting_astc.ktx DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures/)
file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures/painting.ktx ${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures/Bloom-Source-Texture.ktx)
file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures/painting_astc.ktx ${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures/Bloom-Source-Texture-astc.ktx)