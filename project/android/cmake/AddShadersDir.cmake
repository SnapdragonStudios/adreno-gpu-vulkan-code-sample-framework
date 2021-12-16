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

#
# Scan through shaders directory looking for shader source files and generate build commands for them
#
file(GLOB files "shaders/*.vert" "shaders/*.frag" "shaders/*.comp")
foreach(file ${files})
    get_filename_component(OUTPUT_FILENAME ${file} NAME)

    set(OUTPUT_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/Media/Shaders/${OUTPUT_FILENAME}.spv)
    add_custom_command(
        OUTPUT ${OUTPUT_SHADER}
        MAIN_DEPENDENCY ${file}
        COMMAND ${GLSL_VALIDATOR} -V ${file} -o ${OUTPUT_FILENAME}.spv
        COMMAND ${CMAKE_COMMAND} -E rename ${OUTPUT_FILENAME}.spv ${OUTPUT_SHADER}
        COMMENT "Compiling shader ... ${file}" to ${OUTPUT_SHADER}
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

#
# Add shaders (sources) in to a 'Shaders' folder for Visual Studio
#
source_group( "Shader Files" FILES ${SHADERS_SRC} )
