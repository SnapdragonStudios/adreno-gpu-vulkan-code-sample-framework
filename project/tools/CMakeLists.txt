cmake_minimum_required (VERSION 3.25 FATAL_ERROR)  # 3.7 added FindVulkan, 3.10 supported MSVC c++17 (without manually setting flags), 3.19 (maybe before) supported C++20 on Win/Android, 3.21 added Android NDK support with NDK r23 although we are not currently using it!, 3.25 for fetchontent features
cmake_policy(VERSION 3.7)

project(vkSampleFrameworkTools C CXX)

set(CMAKE_CXX_STANDARD 20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD OFF)

# 'Helper' function to call add_sub_directory() and put everything inside a named folder (rather than it littering the top level folder)
define_property(TARGET PROPERTY FOLDER INHERITED BRIEF_DOCS "Set the folder name." FULL_DOCS  "Use to organize targets in an IDE.")
function(add_subdirectory_with_folder _folder_name _folder)
    add_subdirectory(${_folder} ${ARGN})
    set_property(DIRECTORY "${_folder}" PROPERTY FOLDER "${_folder_name}")
endfunction()

# Framework
set(FRAMEWORK_DIR ../../framework)
# add_subdirectory( ${FRAMEWORK_DIR} framework )

add_custom_target(ktxTools ALL)

# KTX-Software library (including tools)
set(KTX_FEATURE_TESTS OFF)
set(KTX_FEATURE_TOOLS ON)
set(KTX_FEATURE_GL_UPLOAD OFF)
set(KTX_FEATURE_VULKAN OFF)
add_subdirectory_with_folder("KTX-Software" ${FRAMEWORK_DIR}/external/KTX-Software KTX-Software EXCLUDE_FROM_ALL)

add_dependencies(ktxTools toktx ktxinfo)

add_custom_command(TARGET ktxTools POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:toktx> ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ktx> ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ktxinfo> ${CMAKE_SOURCE_DIR}
)
