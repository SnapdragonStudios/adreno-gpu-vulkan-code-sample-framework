cmake_minimum_required (VERSION 3.21)

project (vkSampleFramework)

# Turn exceptions off (msvc)
# Done in this top-level class as it needs to apply to libraries and applications globally.
# Even in 2020 exception support removes compiler/optimizer oppertunities to remove 'redundant' instructions (eg look at dissassembly for move constructors).
# Also output debug information on release (Windows) builds so the Release build is debuggable.
if(false)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE} /ZI")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
  set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
  add_definitions(-D_HAS_EXCEPTIONS=0)
endif()
endif()

# Determine which graphics API we can support and set appropriate defines
set(FRAMEWORK_ENABLE_VULKAN OFF)
set(FRAMEWORK_ENABLE_DX12 OFF)
set(FRAMEWORK_WINDOWS_ARM64 OFF)
if(ANDROID)
  # Android target (Vulkan only)
  set(FRAMEWORK_ENABLE_VULKAN ON)
elseif(WIN32)
  # Windows target (Dx12 for sure, maybe Vulkan too)
  set(FRAMEWORK_ENABLE_DX12 ON)
  #if (NOT CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
    set(FRAMEWORK_ENABLE_VULKAN ON)
  #endif()
  if (CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
    set(FRAMEWORK_WINDOWS_ARM64 ON)
  endif()
endif()

message("FRAMEWORK_ENABLE_VULKAN - ${FRAMEWORK_ENABLE_VULKAN}") 
message("FRAMEWORK_ENABLE_DX12   - ${FRAMEWORK_ENABLE_DX12}")

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

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Add in all the child subdirectories
set(FRAMEWORK_DIR ../../framework)
add_subdirectory( ${FRAMEWORK_DIR} framework )
if(FRAMEWORK_ENABLE_VULKAN)
  add_subdirectory(../../samples/empty/ samples/empty)
  add_subdirectory(../../samples/hdrSwapchain/ samples/hdrSwapchain)
  add_subdirectory(../../samples/rotatedCopy/ samples/rotatedCopy)
  add_subdirectory(../../samples/shaderResolveTonemap/ samples/shaderResolveTonemap)
  add_subdirectory(../../samples/hello-gltf/ samples/hello-gltf)
  add_subdirectory(../../samples/SubPass/ samples/SubPass)
  add_subdirectory(../../samples/BloomImageProcessing/ samples/BloomImageProcessing)
  add_subdirectory(../../samples/rayQueryShadows/ samples/rayQueryShadows)
  add_subdirectory(../../samples/rayReflections/ samples/rayReflections)
  add_subdirectory(../../samples/sgsr/ samples/sgsr)
  add_subdirectory(../../samples/sdp-cli/ samples/sdp-cli)
  add_subdirectory(../../samples/sgsr2/ samples/sgsr2)
    add_subdirectory(../../samples/graph_pipelines/ samples/graph_pipelines)
endif()
