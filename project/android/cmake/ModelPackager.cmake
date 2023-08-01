#
# Model Packager
# Copy model files from specified path to media path.
#

function(add_gltf _path)

    if(NOT EXISTS "${_path}.gltf")
        message(FATAL_ERROR "ModelPackager -> Couldn't find .gltf file on given path ${_path}")
        return()
    endif()

    file(COPY "${_path}.gltf" DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Media/Meshes/)

    if(EXISTS "${_path}.bin")
        file(COPY "${_path}.bin" DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Media/Meshes/)
    endif()
    
endfunction()