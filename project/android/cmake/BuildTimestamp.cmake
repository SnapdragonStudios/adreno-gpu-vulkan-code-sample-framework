#
# Generate build (link) time stamp (for the current project).
# Uses a Cmake file per project that gets 'built' as part of the executable's 'PRE_LINK' step but only if the pre-link is run (and so does not trigger extraneous rebuilds)
#

file( WRITE ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.c "const char*const cBUILD_TIMESTAMP = __TIMESTAMP__;\n")
if (true)
    file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.cmake "\n")
    file (APPEND ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.cmake "string(TIMESTAMP TIMEZ UTC)\n")
    file (APPEND ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.cmake "file(WRITE buildTimestamp_${PROJECT_NAME}.c.in \"// Automatically built by build process.  Do NOT check into version control.\\n\\n\")\n")
    file (APPEND ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.cmake "file(APPEND buildTimestamp_${PROJECT_NAME}.c.in \"const char*const cBUILD_TIMESTAMP = \\\"\${TIMEZ}\\\";\\n\\n\")\n")
    file (APPEND ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.cmake "CONFIGURE_FILE(buildTimestamp_${PROJECT_NAME}.c.in ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.c @ONLY)\n")
    add_custom_command (
        OUTPUT ${PROJECT_SOURCE_DIR}/buildTimestamp_${PROJECT_NAME}.c
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.cmake
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.c.in)
    add_custom_command(
        TARGET ${TARGET_NAME} 
        PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.c.in
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target buildTimestamp_${PROJECT_NAME} )
endif()

add_library(buildTimestamp_${PROJECT_NAME} STATIC EXCLUDE_FROM_ALL ${CMAKE_CURRENT_BINARY_DIR}/buildTimestamp_${PROJECT_NAME}.c)
set_target_properties(buildTimestamp_${PROJECT_NAME} PROPERTIES FOLDER "CMakeTimestamp")
target_link_libraries( ${TARGET_NAME} buildTimestamp_${PROJECT_NAME} )
