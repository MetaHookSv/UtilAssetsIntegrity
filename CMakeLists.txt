cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(UtilAssetsIntegrity LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "UtilAssetsIntegrity requires Windows MSVC x86. Configure with -A Win32.")
endif()
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported configurations" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "Select Debug or Release.")
endif()

include(cmake/Dependencies.cmake)
utilassetsintegrity_prepare_dependencies()
include("${VC_LTL_Root}/VC-LTL helper for cmake.cmake")

# FreeImage inherits the parent's VC-LTL settings. Its own install rules are
# excluded; this project installs only its runtime and license notices.
set(USE_VCLTL OFF CACHE BOOL "Separate vendor VC-LTL integration" FORCE)
add_subdirectory("${FREEIMAGE_SOURCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/FreeImage" EXCLUDE_FROM_ALL)
if(NOT TARGET FreeImage)
    message(FATAL_ERROR "FREEIMAGE_SOURCE_PATH must provide the FreeImage CMake target.")
endif()

add_library(utilassetsintegrity_settings INTERFACE)
target_compile_features(utilassetsintegrity_settings INTERFACE cxx_std_20)
target_include_directories(utilassetsintegrity_settings INTERFACE
    "${PROJECT_SOURCE_DIR}/include/Interface"
    "${METAHOOK_SOURCE_PATH}/include"
    "${METAHOOK_SOURCE_PATH}/include/Interface"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/cl_dll"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/engine"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/pm_shared"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/public")
target_compile_definitions(utilassetsintegrity_settings INTERFACE
    WIN32 _WINDOWS UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:_DEBUG> $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(utilassetsintegrity_settings INTERFACE /W3 /permissive- /wd4819 /Zi)

add_library(UtilAssetsIntegrity SHARED
    src/UtilAssetsIntegrity.cpp src/dllmain.cpp
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.cpp")
target_link_libraries(UtilAssetsIntegrity PRIVATE utilassetsintegrity_settings FreeImage)
target_include_directories(UtilAssetsIntegrity PRIVATE "${SCOPEEXIT_SOURCE_PATH}/include")
target_compile_definitions(UtilAssetsIntegrity PRIVATE UTILASSETSINTEGRITY_EXPORTS _USRDLL)
target_compile_options(UtilAssetsIntegrity PRIVATE /MP /sdl
    $<$<CONFIG:Release>:/Gy;/Oi>)
target_link_options(UtilAssetsIntegrity PRIVATE /DEBUG /SUBSYSTEM:WINDOWS /MANIFESTUAC:NO
    $<$<CONFIG:Debug>:/INCREMENTAL>
    $<$<CONFIG:Release>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF>)
set_target_properties(UtilAssetsIntegrity PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

install(TARGETS UtilAssetsIntegrity RUNTIME DESTINATION svencoop/metahook/dlls)
install(FILES "$<TARGET_PDB_FILE:UtilAssetsIntegrity>" DESTINATION svencoop/metahook/dlls)
install(FILES "$<TARGET_FILE:FreeImage>" DESTINATION svencoop/metahook/dlls/FreeImage)
install(FILES include/Interface/IUtilAssetsIntegrity.h DESTINATION include/Interface)
install(FILES README.md README.zh-CN.md LICENSE THIRD-PARTY-NOTICES.md DESTINATION .)
install(FILES docs/build.md DESTINATION docs)
install(FILES "${SCOPEEXIT_SOURCE_PATH}/LICENSE" DESTINATION licenses/ScopeExit)
install(FILES "${METAHOOK_SOURCE_PATH}/LICENSE" DESTINATION licenses/MetaHook)
install(FILES "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.h"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/engine/studio.h" DESTINATION licenses/HLSDK)
install(FILES licenses/VC-LTL/LICENSE DESTINATION licenses/VC-LTL)
install(FILES "${VC_LTL_Root}/Readme.md" DESTINATION licenses/VC-LTL OPTIONAL)
install(FILES "${FREEIMAGE_SOURCE_PATH}/license-fi.txt"
    "${FREEIMAGE_SOURCE_PATH}/license-gplv2.txt" "${FREEIMAGE_SOURCE_PATH}/license-gplv3.txt"
    DESTINATION licenses/FreeImage)
# Some codecs, including JPEG and zlib, put license notices in their README.
file(GLOB_RECURSE freeimage_notices RELATIVE "${FREEIMAGE_SOURCE_PATH}/Source"
    "${FREEIMAGE_SOURCE_PATH}/Source/*LICENSE*" "${FREEIMAGE_SOURCE_PATH}/Source/*license*"
    "${FREEIMAGE_SOURCE_PATH}/Source/*COPYING*" "${FREEIMAGE_SOURCE_PATH}/Source/*copyright*"
    "${FREEIMAGE_SOURCE_PATH}/Source/*README*")
foreach(notice IN LISTS freeimage_notices)
    get_filename_component(notice_directory "${notice}" DIRECTORY)
    install(FILES "${FREEIMAGE_SOURCE_PATH}/Source/${notice}"
        DESTINATION "licenses/FreeImage/Source/${notice_directory}")
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_executable(UtilAssetsIntegritySmokeTests tests/SmokeTests.cpp
        "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.cpp")
    target_link_libraries(UtilAssetsIntegritySmokeTests PRIVATE utilassetsintegrity_settings)
    set_target_properties(UtilAssetsIntegritySmokeTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_dependencies(UtilAssetsIntegritySmokeTests UtilAssetsIntegrity)
    add_test(NAME PublicInterface COMMAND UtilAssetsIntegritySmokeTests
        "$<TARGET_FILE:UtilAssetsIntegrity>" "$<TARGET_FILE:FreeImage>")
endif()
