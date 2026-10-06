set(UTILASSETSINTEGRITY_DEPENDENCY_CACHE_DIR "${PROJECT_SOURCE_DIR}/thirdparty/cache" CACHE PATH "Downloaded binary dependency cache")
set(METAHOOK_SOURCE_PATH "$ENV{METAHOOK_SOURCE_PATH}" CACHE PATH "MetaHook source tree; empty fetches the pinned SDK")
set(FREEIMAGE_SOURCE_PATH "$ENV{FREEIMAGE_SOURCE_PATH}" CACHE PATH "FreeImage source tree; empty fetches the pinned commit")
set(SCOPEEXIT_SOURCE_PATH "$ENV{SCOPEEXIT_SOURCE_PATH}" CACHE PATH "ScopeExit source tree; empty fetches the pinned commit")
set(VC_LTL_Root "$ENV{VC_LTL_Root}" CACHE PATH "Existing VC-LTL binary package; empty downloads the verified package")

function(utilassetsintegrity_require_files name source)
    foreach(required IN LISTS ARGN)
        if(NOT EXISTS "${source}/${required}" OR IS_DIRECTORY "${source}/${required}")
            message(FATAL_ERROR "${name} is missing ${required}: ${source}")
        endif()
    endforeach()
endfunction()

function(utilassetsintegrity_fetch_source name url commit out_var)
    include(FetchContent)
    FetchContent_Declare(${name}
        GIT_REPOSITORY "${url}" GIT_TAG "${commit}"
        GIT_SUBMODULES "" GIT_SUBMODULES_RECURSE FALSE
        # Populate only. FreeImage is configured later, after VC-LTL is applied.
        SOURCE_SUBDIR _utilassetsintegrity_source_only)
    FetchContent_MakeAvailable(${name})
    set(${out_var} "${${name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(utilassetsintegrity_prepare_dependencies)
    set(metahook_files include/metahook.h include/HLSDK/common/interface.h
        include/HLSDK/common/interface.cpp include/HLSDK/engine/studio.h LICENSE)
    set(freeimage_files CMakeLists.txt Source/FreeImage.h
        license-fi.txt license-gplv2.txt license-gplv3.txt)
    set(scopeexit_files include/ScopeExit/ScopeExit.h LICENSE)

    # Validate all explicit paths before any downloads. External trees are read-only.
    foreach(dependency METAHOOK FREEIMAGE SCOPEEXIT)
        if(${dependency}_SOURCE_PATH)
            get_filename_component(source "${${dependency}_SOURCE_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
            string(TOLOWER "${dependency}" lower_name)
            utilassetsintegrity_require_files("${dependency}_SOURCE_PATH" "${source}" ${${lower_name}_files})
            set(${dependency}_SOURCE_PATH "${source}")
        endif()
    endforeach()
    if(VC_LTL_Root)
        get_filename_component(VC_LTL_Root "${VC_LTL_Root}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        utilassetsintegrity_require_files(VC_LTL_Root "${VC_LTL_Root}"
            "VC-LTL helper for cmake.cmake" config/config.cmake
            TargetPlatform/6.0.6000.0/lib/Win32/libucrt.lib)
    endif()

    if(NOT METAHOOK_SOURCE_PATH)
        utilassetsintegrity_fetch_source(utilassetsintegrity_metahook
            https://github.com/MetaHookSv/MetaHook
            4d23b6fecd79dc949aabc2e145480cd1328d4a35 METAHOOK_SOURCE_PATH)
    endif()
    if(NOT FREEIMAGE_SOURCE_PATH)
        utilassetsintegrity_fetch_source(utilassetsintegrity_freeimage
            https://github.com/hzqst/FreeImage_clone
            007c9e4c5d4198a1646b6c5274fd855be9cca7ef FREEIMAGE_SOURCE_PATH)
    endif()
    if(NOT SCOPEEXIT_SOURCE_PATH)
        utilassetsintegrity_fetch_source(utilassetsintegrity_scopeexit
            https://github.com/SergiusTheBest/ScopeExit
            bd345da594a4675d04de663d93d00cb81b6678b2 SCOPEEXIT_SOURCE_PATH)
    endif()
    foreach(dependency METAHOOK FREEIMAGE SCOPEEXIT)
        string(TOLOWER "${dependency}" lower_name)
        utilassetsintegrity_require_files("${dependency}_SOURCE_PATH" "${${dependency}_SOURCE_PATH}" ${${lower_name}_files})
        set(${dependency}_SOURCE_PATH "${${dependency}_SOURCE_PATH}" PARENT_SCOPE)
        message(STATUS "${dependency}_SOURCE_PATH: ${${dependency}_SOURCE_PATH}")
    endforeach()

    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VCLTL.cmake")
    if(NOT VC_LTL_Root)
        set(VC_LTL_Root "${UTILASSETSINTEGRITY_DEPENDENCY_CACHE_DIR}/VC-LTL-5.3.1")
        utilassetsintegrity_prepare_vcltl()
    endif()
    set(VC_LTL_Root "${VC_LTL_Root}" PARENT_SCOPE)
    message(STATUS "VC_LTL_Root: ${VC_LTL_Root}")
endfunction()
