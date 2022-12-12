cmake_minimum_required (VERSION 3.10)

project (shaderResolveTonemap C CXX)
set(CMAKE_CXX_STANDARD 17)  # need new clang (and point gradle to it if we want c++17)

#
# Source files included in this application.
#

set(CPP_SRC code/main/shaderResolveTonemap.cpp
            code/main/shaderResolveTonemap.hpp
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
# Copy required assets to local folders
#
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/scenes/Museum/Museum.bin DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Media/Meshes/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/scenes/Museum/Museum.gltf DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Media/Meshes/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/scenes/Museum/Textures_ktx/ DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures/)
