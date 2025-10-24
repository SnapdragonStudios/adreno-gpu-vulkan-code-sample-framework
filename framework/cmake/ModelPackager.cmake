#
# Model Packager
# Copy model files from specified path to media path or optional destination.
#
function(add_gltf _path)
    cmake_parse_arguments(args "" "DESTINATION" "" ${ARGN})

    if(DEFINED GLOBAL_ASSET_BASE_PATH)
        set(_path "${GLOBAL_ASSET_BASE_PATH}/${_path}")
    endif()

    # Strip .gltf extension if present
    get_filename_component(ext "${_path}" EXT)
    if(ext STREQUAL ".gltf")
        string(REGEX REPLACE "\\.gltf$" "" _path "${_path}")
    endif()

    if(NOT EXISTS "${_path}.gltf")
        message(FATAL_ERROR "ModelPackager -> Couldn't find .gltf file on given path ${_path}")
        return()
    endif()

    set(MODEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Media/Meshes")
    if(DEFINED args_DESTINATION)
        set(MODEL_PATH "${args_DESTINATION}")
    elseif(DEFINED MESH_DESTINATION)
        set(MODEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${MESH_DESTINATION}")
    endif()

    if(NOT EXISTS ${MODEL_PATH})
        file(MAKE_DIRECTORY ${MODEL_PATH})
    endif()

    set(ASSET_CACHE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../build/assets_cache/models")
    if(NOT EXISTS ${ASSET_CACHE_PATH})
        file(MAKE_DIRECTORY ${ASSET_CACHE_PATH})
    endif()

    get_filename_component(model_name ${_path} NAME)

    set(cached_gltf "${ASSET_CACHE_PATH}/${model_name}.gltf")
    set(cached_bin  "${ASSET_CACHE_PATH}/${model_name}.bin")

    # Copy to cache if not already cached
    if(NOT EXISTS ${cached_gltf})
        file(COPY "${_path}.gltf" DESTINATION ${ASSET_CACHE_PATH})
    endif()

    if(EXISTS "${_path}.bin" AND NOT EXISTS ${cached_bin})
        file(COPY "${_path}.bin" DESTINATION ${ASSET_CACHE_PATH})
    endif()

    # Copy from cache to destination
    file(COPY ${cached_gltf} DESTINATION ${MODEL_PATH})

    if(EXISTS ${cached_bin})
        file(COPY ${cached_bin} DESTINATION ${MODEL_PATH})
    endif()
endfunction()