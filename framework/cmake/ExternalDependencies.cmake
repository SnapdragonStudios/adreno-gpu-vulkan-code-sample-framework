#
# Cmake script to pull down external dependencies needed by the Framework project
#
cmake_minimum_required (VERSION 3.25)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(FRAMEWORK_EXTERNALS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
message("FRAMEWORK_EXTERNALS_DIR ${FRAMEWORK_EXTERNALS_DIR}")


if (TRUE)

# build the required externals
if(NOT FRAMEWORK_framework_external)
	message(FATAL_ERROR "Expecting FRAMEWORK_framework_external to be set, did you run \"python configure.py\" to generate \"ConfigLocal.cmake\"?")
else()

	if(FRAMEWORK_framework_external_VulkanMemoryAllocator)
		# Using as 'header only' library (nothing to do here)
	endif()

	if(FRAMEWORK_framework_external_tinyobjloader)
		# Using as 'header only' library (nothing to do here)
	endif()

	if(FRAMEWORK_framework_external_tinygltf)
		# Using as 'header only' library (nothing to do here)
	endif()

	if (FRAMEWORK_framework_external_imgui)
	endif()

	if(FRAMEWORK_framework_external_Vulkan-Headers)
		# Using as 'header only' library (nothing to do here)
	endif()

	if(FRAMEWORK_framework_external_glm)
		# Using as 'header only' library (nothing to do here)
	endif()

	if(FRAMEWORK_framework_external_json)
		# Using as 'header only' library (nothing to do here)
	endif()

	if(FRAMEWORK_framework_external_eigen)
		# Using as 'header only' library (nothing to do here)
	endif()

	if(FRAMEWORK_framework_external_KTX-Software)
    # Add the external KXT-Software project (restrict to ktx_read library)
    set(KTX_FEATURE_TOOLS OFF)
    set(KTX_FEATURE_TESTS OFF)
    set(KTX_FEATURE_STATIC_LIBRARY ON)
    set(KTX_FEATURE_WRITE OFF)
    set(KTX_FEATURE_VULKAN ON)
    set(KTX_FEATURE_GL_UPLOAD OFF)
    set(BASISU_SUPPORT_OPENCL OFF)
    #foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
    #    STRING (REGEX REPLACE "/RTC1" "/O2" ${flag_var} "${${flag_var}}")   # Enable /O2 optimization level in debug builds (ktx library only)
    #endforeach(flag_var)
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)  # disable 'deprecated' warnings from cmake (ktx library throws a deprecation warning)
    add_subdirectory(${FRAMEWORK_EXTERNALS_DIR}/KTX-Software EXCLUDE_FROM_ALL)
    set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)  # restore 'deprecated' warnings from cmake
    #foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
    #    STRING (REGEX REPLACE "/O2" "/RTC1" ${flag_var} "${${flag_var}}")   # Disable /O2 optimizetion (in debug)
    #endforeach(flag_var)
	endif()

	if(FRAMEWORK_framework_external_D3D12MemoryAllocator)
		# Using as 'header only' library (nothing to do here)
	endif()

endif()





else()

include(FetchContent)

option(FRAMEWORK_DOWNLOAD_EXTERNALS "Set download/fetch external repositiories" Yes)

set(FETCHCONTENT_QUIET Off)
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)  # disable 'deprecated' warnings from cmake while we pull the depenencies (some of which have these warnings!)

# Add the external KXT-Software project
function(AddKtxSubdirectory_full)
        #set(_saved_CMAKE_MESSAGE_LOG_LEVEL ${CMAKE_MESSAGE_LOG_LEVEL})
        #set(CMAKE_MESSAGE_LOG_LEVEL NOTICE) # KTX is noisy fetching content, quieting it down!
        set(KTX_FEATURE_TOOLS OFF)
        set(KTX_FEATURE_TESTS OFF)
        set(KTX_FEATURE_STATIC_LIBRARY ON)
        set(KTX_FEATURE_WRITE OFF)
        set(KTX_FEATURE_VULKAN ON)
        set(KTX_FEATURE_GL_UPLOAD OFF)
        set(BASISU_SUPPORT_OPENCL OFF)
        #foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
        #    STRING (REGEX REPLACE "/RTC1" "/O2" ${flag_var} "${${flag_var}}")   # Enable /O2 optimization level in debug builds (ktx library only)
        #endforeach(flag_var)
        #add_subdirectory_with_folder(${ktx-software_SOURCE_DIR} ${ktx-software_SOURCE_DIR} EXCLUDE_FROM_ALL)
        #foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
        #    STRING (REGEX REPLACE "/O2" "/RTC1" ${flag_var} "${${flag_var}}")   # Disable /O2 optimizetion (in debug)
        #endforeach(flag_var)

        message("ktx-software_SOURCE_DIR ${ktx-software_SOURCE_DIR} ktx-software_BINARY_DIR ${ktx-software_BINARY_DIR}")

        add_subdirectory(${ktx-software_SOURCE_DIR} ${ktx-software_BINARY_DIR})
        #set(CMAKE_MESSAGE_LOG_LEVEL ${_saved_CMAKE_MESSAGE_LOG_LEVEL})
endfunction()

# Add the external KXT-Software project (restrict to ktx_read library)
function(AddKtxSubdirectory_partial)
    set(KTX_FEATURE_TOOLS OFF)
    set(KTX_FEATURE_TESTS OFF)
    set(KTX_FEATURE_STATIC_LIBRARY ON)
    set(KTX_FEATURE_WRITE OFF)
    set(KTX_FEATURE_VULKAN ON)
    set(KTX_FEATURE_GL_UPLOAD OFF)
    set(BASISU_SUPPORT_OPENCL OFF)
    #foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
    #    STRING (REGEX REPLACE "/RTC1" "/O2" ${flag_var} "${${flag_var}}")   # Enable /O2 optimization level in debug builds (ktx library only)
    #endforeach(flag_var)
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)  # disable 'deprecated' warnings from cmake (ktx library throws a deprecation warning)
    add_subdirectory(${ktx-software_SOURCE_DIR} EXCLUDE_FROM_ALL)
    set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)  # restore 'deprecated' warnings from cmake
    #foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
    #    STRING (REGEX REPLACE "/O2" "/RTC1" ${flag_var} "${${flag_var}}")   # Disable /O2 optimizetion (in debug)
    #endforeach(flag_var)
    set_property(DIRECTORY "${ktx-software_SOURCE_DIR}" PROPERTY FOLDER "framework/external/KTX-Software")
endfunction()


if(FRAMEWORK_DOWNLOAD_EXTERNALS)
    message("Downloading framework external dependencies")
else()
    set(FETCHCONTENT_FULLY_DISCONNECTED On)
    message("Adding framework external dependencies")
endif()


######################
# TINY OBJECT LOADER #
######################
message(STATUS ">>> Fetching Tiny Object Loader")
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader
    GIT_TAG e39c1737bc61c8dce28be7932cfe839d408e7838
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/tinyobjloader
)
FetchContent_MakeAvailable(tinyobjloader)
set_property(DIRECTORY "${tinyobjloader_SOURCE_DIR}" PROPERTY FOLDER "framework/external/tinyobjloader")

#############
# TINY GLTF #
#############
message(STATUS ">>> Fetching Tiny GLTF")
set(TINYGLTF_BUILD_EXAMPLES OFF CACHE INTERNAL "")
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf
    GIT_TAG 925b83627a136d24411067031893dc8ea661444d
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/tinygltf
)
FetchContent_MakeAvailable(tinygltf)
set_property(DIRECTORY "${tinygltf_SOURCE_DIR}" PROPERTY FOLDER "framework/external/tinygltf")

#########
# IMGUI #
#########
message(STATUS ">>> Fetching ImGui")
FetchContent_Declare(
    imgui
    #GIT_REPOSITORY https://github.com/ocornut/imgui
    #GIT_TAG c71a50deb5ddf1ea386b91e60fa2e4a26d080074
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.87.tar.gz
    URL_MD5 b154fabaa2b3f62e3ec6325be60bbad2
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/imgui
)
# ImGui doesn't have a cmakelists, add_subdirectory(${imgui_SOURCE_DIR} ${imgui_BINARY_DIR}) will not work here, instead
# we add the source files manually in the framework projects
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

#######
# GLM #
#######
message(STATUS ">>> Fetching GLM")
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG 6ad79aae3eb5bf809c30bf1168171e9e55857e45
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/glm
)
FetchContent_MakeAvailable(glm)
set_property(DIRECTORY "${glm_SOURCE_DIR}" PROPERTY FOLDER "framework/external/glm")

########
# JSON #
########
message(STATUS ">>> Fetching JSON")
FetchContent_Declare(
    json
    #GIT_REPOSITORY https://github.com/nlohmann/json
    #GIT_TAG db78ac1d7716f56fc9f1b030b715f872f93964e4
    URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz
    URL_MD5 4b67aba51ddf17c798e80361f527f50e
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/json
)
SET(JSON_BuildTests OFF)
FetchContent_MakeAvailable(json)
set_property(DIRECTORY "${json_SOURCE_DIR}" PROPERTY FOLDER "framework/external/json")

###########################
# VULKAN MEMORY ALLOCATOR #
###########################
message(STATUS ">>> Fetching Vulkan Memory Allocator")
FetchContent_Declare(
    vma
    #GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    #GIT_TAG db4c1639bf30c51bbddcd813c6521b3473afa1a1
    URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.0.1.tar.gz
    URL_MD5 8571f3def0ff86f228e2864c907ba0b3
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/VulkanMemoryAllocator
)
# Using as 'header only' library
FetchContent_GetProperties(vma)
if(NOT vma_POPULATED)
    FetchContent_Populate(vma)
endif()

##################
# VULKAN HEADERS #
##################
message(STATUS ">>> Fetching Vulkan Headers")
FetchContent_Declare(
    Vulkan-Headers
    #GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers
    #GIT_TAG cbcad3c0587dddc768d76641ea00f5c45ab5a278
    URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.3.296.0.tar.gz
    URL_MD5 c1e5eaee17f6dfde2e5b843d1e9380d4
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/Vulkan-Headers
)
FetchContent_MakeAvailable(Vulkan-Headers)
set_property(DIRECTORY "${vulkan-headers_SOURCE_DIR}" PROPERTY FOLDER "framework/external/Vulkan-Headers")

#########
# EIGEN #
#########
message(STATUS ">>> Fetching Eigen")
FetchContent_Declare(
    eigen
    #GIT_REPOSITORY https://gitlab.com/libeigen/eigen
    #GIT_TAG 3147391d946bb4b6c68edd901f2add6ac1f31f8c
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    URL_MD5 4c527a9171d71a72a9d4186e65bea559
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/eigen
)
# Eigen is header-only
FetchContent_GetProperties(eigen)
if(NOT eigen_POPULATED)
    FetchContent_Populate(eigen)
endif()

################
# KTX SOFTWARE #
################
message(STATUS ">>> Fetching KTX Software")
FetchContent_Declare(
    KTX-Software
    #GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software
    #GIT_TAG 7149d4fc08bb00c070883d174e46e102a6974a8c
    URL https://github.com/KhronosGroup/KTX-Software/archive/refs/tags/v4.1.0.tar.gz
    URL_MD5 b35fc412cdb3a00aa92aadcdd1e5f004
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/KTX-Software
)
# Manually add the KTX library
FetchContent_GetProperties(KTX-Software)
if(NOT ktx-software_POPULATED)
    FetchContent_Populate(KTX-Software)
	AddKtxSubdirectory_partial()
endif()

#######################
# DX Memory Allocator #
#######################
message(STATUS ">>> DX Memory Allocator")
FetchContent_Declare(
    D3D12MemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator
    GIT_TAG 7597f717c7b32b74d263009ecc15985b517585c7
    SOURCE_DIR ${FRAMEWORK_EXTERNALS_DIR}/D3D12MemoryAllocator
)
FetchContent_MakeAvailable(D3D12MemoryAllocator)
set_property(DIRECTORY "${d3d12memoryallocator_SOURCE_DIR}" PROPERTY FOLDER "framework/external/D3D12MemoryAllocator")


set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)  # restore 'deprecated' warnings from cmake


endif()




