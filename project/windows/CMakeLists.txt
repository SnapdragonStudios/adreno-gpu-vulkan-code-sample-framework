cmake_minimum_required (VERSION 3.10)

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

set(FRAMEWORK_DIR ../../framework)
add_subdirectory( ${FRAMEWORK_DIR} framework )
add_subdirectory(../../samples/empty/ samples/empty)
