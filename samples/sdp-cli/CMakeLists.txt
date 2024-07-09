cmake_minimum_required (VERSION 3.21)

project (sdp_cli C CXX)
set(CMAKE_CXX_STANDARD 20)

#
# Source files included in this application.
#
set(CPP_SRC code/main/application.cpp
            code/main/application.hpp

            # Helpers
            code/main/helpers/imgui_extensions.cpp
            code/main/helpers/imgui_extensions.hpp
            code/main/helpers/console_common.cpp
            code/main/helpers/console_common.hpp
            code/main/helpers/console_helper.cpp
            code/main/helpers/console_helper.hpp
            code/main/helpers/module_interface.cpp
            code/main/helpers/module_interface.hpp
            code/main/helpers/numerical_aggregator.cpp
            code/main/helpers/numerical_aggregator.hpp

            # Module (SDP CLI)
            code/main/modules/sdp_cli/sdp_cli.cpp
            code/main/modules/sdp_cli/sdp_cli.hpp
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
