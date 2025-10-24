#
# Parse and build shader alias
#
# Inputs:
#   INPUT_ALIAS
#   OUTPUT_SHADER
#   OUTPUT_SHADER_DEP

file(READ ${INPUT_ALIAS} ALIAS_JSON)

string(JSON INPUT_SHADER GET ${ALIAS_JSON} Shader)
string(JSON INPUT_DEFINES GET ${ALIAS_JSON} Defines)
string(JSON TARGET_ENV ERROR_VARIABLE TARGET_ENV_JSON_ERROR GET ${ALIAS_JSON} TargetEnv)
if(NOT ${TARGET_ENV_JSON_ERROR} STREQUAL "NOTFOUND")
    set(TARGET_ENV "vulkan1.1")
endif()

string(JSON STAGE ERROR_VARIABLE STAGE_JSON_ERROR GET ${ALIAS_JSON} Stage)
if(NOT ${STAGE_JSON_ERROR} STREQUAL "NOTFOUND")
    set(STAGE_OPT "")
else()
    set(STAGE_OPT "-S ${STAGE}")
endif()

# expand out the "Defines: []" JSON array
set(DEFINES "")
string(JSON INPUT_DEFINES_COUNT LENGTH ${INPUT_DEFINES})
if(INPUT_DEFINES_COUNT GREATER 0)
    math(EXPR INPUT_DEFINES_COUNT "${INPUT_DEFINES_COUNT} - 1")
    foreach(DEFINE_IDX RANGE ${INPUT_DEFINES_COUNT})
      string(JSON DEFINE GET ${INPUT_DEFINES} ${DEFINE_IDX})
      list(APPEND DEFINES "-D${DEFINE}")
    endforeach()
endif()
string(REPLACE ";" " " DEFINES "${DEFINES}")

message(VERBOSE "Defines ${DEFINES}")

cmake_path(REMOVE_FILENAME INPUT_ALIAS OUTPUT_VARIABLE INPUT_ALIAS_PATH)
cmake_path(APPEND I ${INPUT_ALIAS_PATH} ${INPUT_SHADER})
set(INPUT_SHADER ${I})
cmake_path(GET INPUT_SHADER EXTENSION LAST_ONLY INPUT_SHADER_EXT)

#message("Shader ${INPUT_SHADER}")
#message("Defines ${INPUT_DEFINES}")
#message("GLSL_VALIDATOR ${GLSL_VALIDATOR}")
#message("Shader ext ${INPUT_SHADER_EXT}")

if ( "${INPUT_SHADER_EXT}" STREQUAL ".hlsl" )
    # HLSL
    message(VERBOSE "COMMAND ${DXC_EXE} ${SHADER_INCLUDE} -T cs_6_7 -spirv -Wno-conversion -fspv-target-env=${TARGET_ENV} ${DEFINES} -enable-16bit-types -E main -Fo ${OUTPUT_SHADER} -MF ${OUTPUT_SHADER_DEP} ${INPUT_SHADER}")
    separate_arguments(ARGS NATIVE_COMMAND PROGRAM SEPARATE_ARGS "${DXC_EXE} ${SHADER_INCLUDE} -T cs_6_7 -spirv -Wno-conversion -fspv-target-env=${TARGET_ENV} ${DEFINES} -enable-16bit-types -E main -Fo ${OUTPUT_SHADER} ${INPUT_SHADER}")
    execute_process(
        COMMAND ${ARGS} "-MF ${OUTPUT_SHADER_DEP}"
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND ${ARGS} 
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
else()
    # GLSL
    message(VERBOSE "COMMAND ${GLSL_VALIDATOR} ${SHADER_INCLUDE} -I. -V --quiet ${STAGE_OPT} --target-env ${TARGET_ENV} ${DEFINES} ${INPUT_SHADER} -o ${OUTPUT_SHADER} --depfile ${OUTPUT_SHADER_DEP}")
    separate_arguments(ARGS NATIVE_COMMAND PROGRAM SEPARATE_ARGS "${GLSL_VALIDATOR} ${SHADER_INCLUDE} -I. -V --quiet ${STAGE_OPT} --target-env ${TARGET_ENV} ${DEFINES} ${INPUT_SHADER} -o ${OUTPUT_SHADER} --depfile ${OUTPUT_SHADER_DEP}")
    execute_process(
        COMMAND ${ARGS} 
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

