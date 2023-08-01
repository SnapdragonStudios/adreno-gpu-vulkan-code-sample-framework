#
# Build shaders
# Add everything with .frag .vert .comp extension from the shaders/ directory and build using Vulkan shader compiler.
#

# Make sure we have the Vulkan compiler
find_program(
	GLSL_VALIDATOR
	glslangValidator
	DOC "Vulkan Shader Compiler (glslangValidator) (is Vulkan SDK installed?)"
	REQUIRED
)

# Make sure we have the DXC compiler.  See if it is installed alngside Vulkan first.
if(DEFINED ENV{VULKAN_SDK})
    cmake_path(SET VULKAN_SDK_PATH NORMALIZE $ENV{VULKAN_SDK})
    find_program(
        DXC_EXE
        dxc
        HINTS ${VULKAN_SDK_PATH}
        PATH_SUFFIXES bin
        NO_DEFAULT_PATH
        DOC "Microsoft Shader compiler (dxc) (is Vulkan SDK installed?)"
        OPTIONAL
    )
endif()

# We couldnt find dxc installed with Vulkan, look for it on the path (should find the Windows SDK version)
find_program(
    DXC_EXE
    dxc
    DOC "Microsoft Shader compiler (dxc) (is Vulkan SDK installed?)"
    REQUIRED
)

# Runs the command to get the pluginval version (more recent versions of dcx support --version, older ones dont and print the version inside -help)
if (NOT DEFINED ENV{DXC_VERSION})
    message("DCX compiler found at: ${DXC_EXE}")
    execute_process(COMMAND ${DXC_EXE} --help
                    OUTPUT_VARIABLE DXC_VERSION_RAW
                    ERROR_VARIABLE DXC_VERSION_RAW)
    string(REGEX MATCH "Version: ([^\r\n]+)[\r\n]"
           DXC_VERSION ${DXC_VERSION_RAW})
    string(REGEX REPLACE "Version: (.*)"
           "\\1"
           DXC_VERSION ${DXC_VERSION})
    message( "DXC version: ${DXC_VERSION}" )
    set(ENV{DXC_VERSION} ${DXC_VERSION})
endif()


# Custom shader include direcotry
if(DEFINED SHADER_INCLUDE)
  list(TRANSFORM SHADER_INCLUDE PREPEND "-I")
endif()

# Ensure we have a place to put the .d dependency files emitted by the compiler
set(DEPENDS_PATH ${CMAKE_CURRENT_BINARY_DIR}/Media/Shaders/)
file(MAKE_DIRECTORY ${DEPENDS_PATH})

#
# Scan through shaders directory looking for shader source files and generate build commands for them
#
file(GLOB files "shaders/*.vert" "shaders/*.frag" "shaders/*.comp")
foreach(file ${files})
    get_filename_component(OUTPUT_FILENAME ${file} NAME)

    set(OUTPUT_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/Media/Shaders/${OUTPUT_FILENAME}.spv)
    set(OUTPUT_SHADER_DEP ${DEPENDS_PATH}${OUTPUT_FILENAME}.spv.d)
    add_custom_command(
        OUTPUT ${OUTPUT_SHADER}
        MAIN_DEPENDENCY ${file}
        DEPFILE ${OUTPUT_SHADER_DEP}
        COMMAND echo Compiling shader ... ${file} to ${OUTPUT_SHADER}
        COMMAND ${GLSL_VALIDATOR} ${SHADER_INCLUDE} -I. -V --quiet --target-env vulkan1.1 ${file} -o ${OUTPUT_FILENAME}.spv --depfile ${OUTPUT_SHADER_DEP}
        COMMAND ${CMAKE_COMMAND} -E rename ${OUTPUT_FILENAME}.spv ${OUTPUT_SHADER}
    )
    list(APPEND SHADERS_SRC ${file})

    unset(OUTPUT_SHADER)
    unset(OUTPUT_FILENAME)
endforeach()

# Ray Tracing shaders need to target Vulkan 1.2
file(GLOB files "shaders/*.rgen" "shaders/*.rint" "shaders/*.rahit" "shaders/*.rchit" "shaders/*.rmiss" "shaders/*.rcall")
foreach(file ${files})
    get_filename_component(OUTPUT_FILENAME ${file} NAME)

    set(OUTPUT_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/Media/Shaders/${OUTPUT_FILENAME}.spv)
    set(OUTPUT_SHADER_DEP ${DEPENDS_PATH}${OUTPUT_FILENAME}.spv.d)
    add_custom_command(
        OUTPUT ${OUTPUT_SHADER}
        MAIN_DEPENDENCY ${file}
        DEPFILE ${OUTPUT_SHADER_DEP}
        COMMAND echo Compiling shader ... ${file} to ${OUTPUT_SHADER}
        COMMAND ${GLSL_VALIDATOR} ${SHADER_INCLUDE} -I. -V --quiet --target-env spirv1.4 ${file} -o ${OUTPUT_FILENAME}.spv --depfile ${OUTPUT_SHADER_DEP}
        COMMAND ${CMAKE_COMMAND} -E rename ${OUTPUT_FILENAME}.spv ${OUTPUT_SHADER}
    )
    list(APPEND SHADERS_SRC ${file})

    unset(OUTPUT_SHADER)
    unset(OUTPUT_FILENAME)
endforeach()

# Hlsl files (compiled to SPIR-V).  Entry point assumed to me "main".
file(GLOB files "shaders/*.comp.hlsl")
foreach(file ${files})
    get_filename_component(OUTPUT_FILENAME ${file} NAME)

    set(OUTPUT_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/Media/Shaders/${OUTPUT_FILENAME}.spv)
    set(OUTPUT_SHADER_DEP ${DEPENDS_PATH}${OUTPUT_FILENAME}.spv.d)
    add_custom_command(
        OUTPUT ${OUTPUT_SHADER}
        MAIN_DEPENDENCY ${file}
        DEPFILE ${OUTPUT_SHADER_DEP}
        COMMAND echo Compiling shader ... ${file} to ${OUTPUT_SHADER}  ${DXC_EXE}
        COMMAND ${DXC_EXE} ${SHADER_INCLUDE} -I. -T cs_6_7 -spirv -fspv-target-env=vulkan1.1 -enable-16bit-types -E main -Fo ${OUTPUT_FILENAME}.spv ${file}
        COMMAND ${CMAKE_COMMAND} -E rename ${OUTPUT_FILENAME}.spv ${OUTPUT_SHADER}
    )
    list(APPEND SHADERS_SRC ${file})

    unset(OUTPUT_SHADER)
    unset(OUTPUT_FILENAME)
endforeach()


file(GLOB files "shaders/*.json")
foreach(file ${files})
    get_filename_component(OUTPUT_FILENAME ${file} NAME)

    set(OUTPUT_JSON ${CMAKE_CURRENT_SOURCE_DIR}/Media/Shaders/${OUTPUT_FILENAME})
    add_custom_command(
        OUTPUT ${OUTPUT_JSON}
        MAIN_DEPENDENCY ${file}
        COMMAND ${CMAKE_COMMAND} -E copy ${file} ${OUTPUT_JSON}
        COMMENT "Copying ... ${file}" to ${OUTPUT_JSON}
    )
    list(APPEND SHADERS_SRC ${file})

    unset(OUTPUT_JSON)
endforeach()

# Aliased shaders compile copies of other shaders (but allow for #define setting)
file(GLOB_RECURSE files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "shaders/*.alias")
foreach(file ${files})
    set(INPUT_ALIAS ${CMAKE_CURRENT_SOURCE_DIR}/${file})
    get_filename_component(OUTPUT_FILENAME ${file} NAME_WLE)
    set(OUTPUT_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/Media/Shaders/${OUTPUT_FILENAME}.spv)
    set(OUTPUT_SHADER_DEP ${DEPENDS_PATH}${OUTPUT_FILENAME}.spv.d)
    cmake_path(NATIVE_PATH INPUT_ALIAS NORMALIZE INPUT_ALIAS_NATIVE)
    cmake_path(NATIVE_PATH OUTPUT_SHADER NORMALIZE OUTPUT_SHADER_NATIVE)
    cmake_path(NATIVE_PATH OUTPUT_SHADER_DEP NORMALIZE OUTPUT_SHADER_DEP_NATIVE)

    add_custom_command(
        OUTPUT ${OUTPUT_SHADER}
        MAIN_DEPENDENCY ${INPUT_ALIAS}
        DEPFILE ${OUTPUT_SHADER_DEP}
        COMMENT "Aliasing ... ${INPUT_ALIAS}" to ${OUTPUT_SHADER} (dependency file ${OUTPUT_SHADER_DEP})
        COMMAND echo Aliasing ... ${INPUT_ALIAS} to ${OUTPUT_SHADER} (dependency file ${OUTPUT_SHADER_DEP})
        COMMAND ${CMAKE_COMMAND} -DINPUT_ALIAS=${INPUT_ALIAS} -DOUTPUT_SHADER=${OUTPUT_SHADER} -DOUTPUT_SHADER_DEP=${OUTPUT_SHADER_DEP} -DGLSL_VALIDATOR=${GLSL_VALIDATOR} -DSHADER_INCLUDE=${SHADER_INCLUDE} -P ${CMAKE_CURRENT_LIST_DIR}/CompileAlias.cmake
    )
    list(APPEND SHADERS_SRC ${file})

    unset(OUTPUT_JSON)
endforeach()

#
# Add shaders (sources) in to a 'Shaders' folder for Visual Studio
#
source_group( "Shader Files" FILES ${SHADERS_SRC} )
