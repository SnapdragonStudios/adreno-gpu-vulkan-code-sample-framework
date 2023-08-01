#
# Texture Packager
# Convert PNG textures from the specified path into its media equivalent.
#

function(add_textures_from_path _path)

    # Attempt to find the converter tool, else display warning message
    SET(CONVERTER_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/../../project/tools/toktx.exe")
    if(NOT EXISTS ${CONVERTER_TOOL})
        message(WARNING "TexturePackager -> Texture converter tool wasn't found, sample textures will not be generated (Run '03_BuildTools.bat' if that's desired) - ${CONVERTER_TOOL}")
        return()
    endif()

    # Ensure the texture path exist
    SET(TEXTURE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures")
    if(NOT EXISTS ${TEXTURE_PATH})
        file(MAKE_DIRECTORY ${TEXTURE_PATH})
    endif()

    MESSAGE("Converting textures from path: '${_path}'")

    # For each PNG file -> Convert using the tool
    file(GLOB png_textures "${_path}/*.png")
    foreach(file ${png_textures})

        get_filename_component(OUTPUT_FILENAME ${file} NAME_WE)

        SET(OUTPUT_PATH "${TEXTURE_PATH}/${OUTPUT_FILENAME}.ktx")

        SET(PARAMS 
            "--genmipmap" 
            --scale 1.0 
            "--verbose" 
            "${OUTPUT_PATH}" 
            "${file}"
        )

        if(EXISTS ${OUTPUT_PATH})
            # MESSAGE("Ignoring already converted texture: '${OUTPUT_FILENAME}'")
            continue()
        endif()

        # MESSAGE("Converting texture: '${OUTPUT_FILENAME}' to KTX")

        execute_process(COMMAND "${CONVERTER_TOOL}" ${PARAMS} ERROR_VARIABLE CONV_ERROR RESULT_VARIABLE CONV_RETVAL)

        if(CONV_ERROR)    
            message(WARNING "TexturePackager -> ${CONV_ERROR}")
        endif()

    endforeach()

    # For each KTX file -> Copy
    file(GLOB ktx_textures "${_path}/*.ktx")
    foreach(file ${ktx_textures})

        get_filename_component(OUTPUT_FILENAME ${file} NAME_WE)
        SET(OUTPUT_PATH "${TEXTURE_PATH}/${OUTPUT_FILENAME}.ktx")

        if(EXISTS ${OUTPUT_PATH})
            # MESSAGE("Ignoring existing texture: '${OUTPUT_FILENAME}'")
            continue()
        endif()    

        SET(OUTPUT_PATH "${TEXTURE_PATH}/")

        # MESSAGE("Copying KTX texture: '${OUTPUT_FILENAME}'")
        file(COPY ${file} DESTINATION ${OUTPUT_PATH})

    endforeach()    

endfunction()

function(add_texture _texture_path)
    
    if(NOT EXISTS ${_texture_path})
        message(WARNING "TexturePackager -> Requested texture doesn't exist: ${_texture_path}")
        return()
    endif()

    # Ensure the texture path exist
    SET(TEXTURE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Media/Textures")
    if(NOT EXISTS ${TEXTURE_PATH})
        file(MAKE_DIRECTORY ${TEXTURE_PATH})
    endif()

    get_filename_component(OUTPUT_FILENAME ${_texture_path} NAME_WE)
    get_filename_component(OUTPUT_EXT ${_texture_path} EXT)
    SET(COPY_OUTPUT_PATH "${TEXTURE_PATH}/")
    SET(DST_OUTPUT_PATH "${TEXTURE_PATH}/${OUTPUT_FILENAME}.ktx")

    if(EXISTS ${DST_OUTPUT_PATH})
        # MESSAGE("Ignoring existing texture: '${OUTPUT_FILENAME}'")
        return()
    endif()

    if(OUTPUT_EXT STREQUAL ".ktx")
        MESSAGE("Copying KTX texture: '${OUTPUT_FILENAME}'")
        file(COPY ${_texture_path} DESTINATION ${COPY_OUTPUT_PATH})
        return()
    endif()

    SET(CONVERTER_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/../../project/tools/toktx.exe")

    if(NOT EXISTS ${CONVERTER_TOOL})
        message(WARNING "TexturePackager -> Texture converter tool wasn't found, sample textures will not be generated (Run '03_BuildTools.bat' if that's desired) - ${CONVERTER_TOOL}")
        return()
    endif()

    MESSAGE("Converting texture: '${OUTPUT_FILENAME}' to KTX")

    SET(PARAMS 
        "--genmipmap" 
        --scale 1.0 
        "--verbose" 
        "${DST_OUTPUT_PATH}" 
        "${_texture_path}"
    )

    execute_process(COMMAND "${CONVERTER_TOOL}" ${PARAMS} ERROR_VARIABLE CONV_ERROR RESULT_VARIABLE CONV_RETVAL)

    if(CONV_ERROR)    
        message(WARNING "TexturePackager -> ${CONV_ERROR}")
    endif()

endfunction()