#
# Build the application, linking against the framework.
#

# Input:
#  CPP_SRC       - list of cpp/hpp files
#  SHADERS_SRC   - list of 'shader' (.vert, .frag, .comp) source files
#  FRAMEWORK_LIB    - name of the framework to link against (eg framework_vulkan)
#  NATVIS_SCHEMA - list of application specific Visual Studio visualization schemas (for debuffer)
#  FRAMEWORK_LIB - name of the helper framework library (eg framework_vulkan or framework_dx12)
#  PROJECT_NAME  - name of the application being compiled (from the 'project(...)' command)

if(NOT DEFINED FRAMEWORK_LIB)
    set(FRAMEWORK_LIB framework_vulkan)
endif()

# Windows and Android differ in terms of output.
# Windows generates an executable, Android generates a library (that the AndroidManifest references)
if(WIN32)
    set( TARGET_NAME ${PROJECT_NAME} )
    add_executable( ${TARGET_NAME} WIN32 ${CPP_SRC} ${SHADERS_SRC} ${NATVIS_SCHEMA})
    target_compile_definitions( ${TARGET_NAME} PRIVATE OS_WINDOWS;_CRT_SECURE_NO_WARNINGS )
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
elseif(ANDROID)
    set( TARGET_NAME native-lib )
    add_library( ${TARGET_NAME} SHARED ${CPP_SRC} ${SHADERS_SRC} )
    target_compile_definitions(${TARGET_NAME} PRIVATE OS_ANDROID)

    # Android also needs app-glue generating (and linking before the framework)
    add_library( app-glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c )
    target_link_libraries( ${TARGET_NAME} app-glue )
    # Export ANativeActivity_onCreate(), see https://github.com/android-ndk/ndk/issues/381.
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
    # FRAMEWORK_DIR is set by the calling gradle script (needed for Android)
    add_subdirectory( ${FRAMEWORK_DIR} framework )
else()
    message(FATAL_ERROR "(currently) Unsupported platform")
endif()

if(NOT DEFINED FRAMEWORK_LIB)
    set(FRAMEWORK_LIB framework_vulkan)
endif()

#
# Link the framework (to our application)
#
target_link_libraries( ${TARGET_NAME} ${FRAMEWORK_LIB} )

include_directories(.)

set(DEFAULT_LOCAL_SHADER_DESTINATION  "build/Media/Shaders"  CACHE INTERNAL "Default local shader destionation")
set(DEFAULT_LOCAL_MESH_DESTINATION    "build/Media/Meshes"   CACHE INTERNAL "Default local mesh destionation")
set(DEFAULT_LOCAL_TEXTURE_DESTINATION "build/Media/Textures" CACHE INTERNAL "Default local texture destionation")
set(DEFAULT_LOCAL_MISC_DESTINATION    "build/Media/Misc"     CACHE INTERNAL "Default local misc destionation")

macro(inject_root_asset_path)
    if(${ARGC} GREATER 0)
        set(GLOBAL_ASSET_BASE_PATH "${ARGV0}" CACHE INTERNAL "Global asset base path for asset packaging")
    else()
        set(GLOBAL_ASSET_BASE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../framework/external/GameSampleAssets" CACHE INTERNAL "Global asset base path for asset packaging")
    endif()
endmacro()

macro(register_local_asset_path var_name relative_path)
    set(${var_name} "${relative_path}" CACHE INTERNAL "Global asset local path for asset packaging")
    add_compile_definitions(${var_name}_PATH="${relative_path}")
endmacro()