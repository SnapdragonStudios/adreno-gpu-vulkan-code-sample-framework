#
# Build the application, linking against the framework.
#

# Input:
#  CPP_SRC       - list of cpp/hpp files
#  SHADERS_SRC   - list of 'shader' (.vert, .frag, .comp) source files
#  NATVIS_SCHEMA - list of application specific Visual Studio visualization schemas (for debuffer)
#  PROJECT_NAME  - name of the application being compiled (from the 'project(...)' command)
#  FRAMEWORK_LIB - name of the helper framework library (eg framework_vulkan or framework_dx12)


# Windows and Android differ in terms of output.
# Windows generates an executable, Android generates a library (that the AndroidManifest references)
if(WIN32)
    set( TARGET_NAME ${PROJECT_NAME} )
    add_executable( ${TARGET_NAME} WIN32 ${CPP_SRC} ${SHADERS_SRC} ${NATVIS_SCHEMA})
    add_dependencies( ${TARGET_NAME} buildTimestamp )
    target_compile_definitions( ${TARGET_NAME} PRIVATE OS_WINDOWS;_CRT_SECURE_NO_WARNINGS )
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
elseif(ANDROID)
    set( TARGET_NAME native-lib )
    add_library( ${TARGET_NAME} SHARED ${CPP_SRC} ${SHADERS_SRC} )
    target_compile_definitions(${TARGET_NAME} PRIVATE OS_ANDROID)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-nullability-completeness;-Wno-deprecated-volatile;-Wno-deprecated-anon-enum-enum-conversion)

    # Generate build time stamp as 2 part process.  CMake file that exe depends upon and the header file that cmake writes the header to be included by code.  This way we only rebuild what is needed.
    file (WRITE ${CMAKE_BINARY_DIR}/buildTimestamp.cmake "string(TIMESTAMP TIMEZ UTC)\n")
    file (APPEND ${CMAKE_BINARY_DIR}/buildTimestamp.cmake "file(WRITE  ../../buildtimestamp.h \"#ifndef _BUILDTIMESTAMP_H_\\n\")\n")
    file (APPEND ${CMAKE_BINARY_DIR}/buildTimestamp.cmake "file(APPEND ../../buildTimestamp.h \"#define _BUILDTIMESTAMP_H_\\n\\n\")\n")
    file (APPEND ${CMAKE_BINARY_DIR}/buildTimestamp.cmake "file(APPEND ../../buildTimestamp.h \"// Automatically built by build process.  Do NOT check into version control.\\n\\n\")\n")
    file (APPEND ${CMAKE_BINARY_DIR}/buildTimestamp.cmake "file(APPEND ../../buildTimestamp.h \"#define BUILD_TIMESTAMP \\\"\${TIMEZ}\\\"\\n\\n\")\n")
    file (APPEND ${CMAKE_BINARY_DIR}/buildTimestamp.cmake "file(APPEND ../../buildTimestamp.h \"#endif // _BUILDTIMESTAMP_H_\\n\")\n")
    add_custom_target (
        buildTimestamp
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/buildTimestamp.cmake
        ADD_DEPENDENCIES ${CMAKE_BINARY_DIR}/buildTimestamp.cmake)
    add_dependencies( ${TARGET_NAME} buildTimestamp )

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
