project (samples C CXX)

set(CMAKE_CXX_STANDARD 20)

# From the list of projects determine which are enabled (by ConfigLocal.cmake - which sets a variable per sample)
# if a variable is not found for a sample it is assumed to be enabled.
function(add_sample_subdirectory _SAMPLE_FOLDER _SAMPLE_NAME)
  if(FRAMEWORK_tests AND (NOT DEFINED FRAMEWORK_tests_${_SAMPLE_NAME} OR FRAMEWORK_tests_${_SAMPLE_NAME}))
    if(NOT DEFINED FRAMEWORK_tests_${_SAMPLE_NAME})
      message(STATUS "Skipping test: " ${_SAMPLE_NAME} " (config flag not found - please add to Config.txt and re-run configuration script)")
      return()
    else()
      message(STATUS "Adding     test: " ${_SAMPLE_NAME})
    endif()
    add_subdirectory(${_SAMPLE_FOLDER} ${_SAMPLE_NAME})
  else()
    message(STATUS "Skipping   test: " ${_SAMPLE_NAME})
  endif()
endfunction()

# Automatically discover test projects
file(GLOB SAMPLE_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} */)

foreach(SAMPLE_DIR ${SAMPLE_DIRS})
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SAMPLE_DIR}/CMakeLists.txt" OR
     EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SAMPLE_DIR}/cmakelists.txt")
    add_sample_subdirectory(${SAMPLE_DIR} ${SAMPLE_DIR})
  endif()
endforeach()