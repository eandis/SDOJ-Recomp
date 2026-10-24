# saidaioujou_recomp_tu1 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(saidaioujou_recomp_tu1 LANGUAGES C CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk"
    CACHE PATH "Path to the bundled ReXGlue SDK")

include(generated/rexglue.cmake)

include(FetchContent)
FetchContent_Declare(extract_xiso
    GIT_REPOSITORY https://github.com/XboxDev/extract-xiso.git
    GIT_TAG 3f5b62cfe68f000b0e3c8a30104973f3a297948e
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(extract_xiso)
if(WIN32)
    target_compile_definitions(extract-xiso PRIVATE WIN32)
endif()

# Sources
set(SAIDAIOUJOURECOMPTU1_SOURCES
    src/game_setup.cpp
    src/muteki.cpp
    src/popups.cpp
    src/settings.cpp
    src/update_checker.cpp
    src/main.cpp
)

if(WIN32)
    set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
    configure_file(resources/app_icon.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc" @ONLY)
    add_executable(saidaioujou_recomp_tu1 WIN32
        ${SAIDAIOUJOURECOMPTU1_SOURCES}
        "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc"
    )
else()
    add_executable(saidaioujou_recomp_tu1 ${SAIDAIOUJOURECOMPTU1_SOURCES})
endif()

rexglue_setup_target(saidaioujou_recomp_tu1)
set(SDOJ_RELEASE_VERSION "v1.51.2" CACHE STRING
    "Current release version; empty disables the update checker for development builds")
if(SDOJ_RELEASE_VERSION AND NOT SDOJ_RELEASE_VERSION MATCHES "^v?[0-9]+\\.[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?$")
    message(FATAL_ERROR "SDOJ_RELEASE_VERSION must be a numeric release version or empty")
endif()
target_compile_definitions(saidaioujou_recomp_tu1 PRIVATE
    SDOJ_RELEASE_VERSION="${SDOJ_RELEASE_VERSION}")
target_include_directories(saidaioujou_recomp_tu1 PRIVATE
    "${REXSDK_DIR}/thirdparty/inja/third_party/include")
set_target_properties(saidaioujou_recomp_tu1 PROPERTIES
    OUTPUT_NAME "DoDonPachi SaiDaiOuJou"
)

set(_sdoj_extractor_stage_dir
    "$<TARGET_FILE_DIR:saidaioujou_recomp_tu1>/tools/extract-xiso")
add_custom_target(sdoj_stage_extract_xiso
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${_sdoj_extractor_stage_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:extract-xiso>"
        "${_sdoj_extractor_stage_dir}/$<TARGET_FILE_NAME:extract-xiso>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${extract_xiso_SOURCE_DIR}/LICENSE.TXT"
        "${_sdoj_extractor_stage_dir}/LICENSE.TXT"
    VERBATIM
)
add_dependencies(sdoj_stage_extract_xiso extract-xiso)
add_dependencies(saidaioujou_recomp_tu1 sdoj_stage_extract_xiso)

if(WIN32)
    set(SDOJ_VC_REDIST_DIR "" CACHE PATH
        "Visual C++ redist root, version directory, architecture directory, or CRT directory")
    set(_sdoj_vc_redist_root "${SDOJ_VC_REDIST_DIR}")
    if(NOT _sdoj_vc_redist_root AND DEFINED ENV{VCToolsRedistDir})
        set(_sdoj_vc_redist_root "$ENV{VCToolsRedistDir}")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
        set(_sdoj_vc_redist_arch arm64)
    else()
        set(_sdoj_vc_redist_arch x64)
    endif()

    set(_sdoj_vc_crt_dirs)
    if(_sdoj_vc_redist_root)
        get_filename_component(_sdoj_vc_redist_dir_name
            "${_sdoj_vc_redist_root}" NAME)
        if(_sdoj_vc_redist_dir_name MATCHES "^Microsoft\\.VC.*\\.CRT$")
            list(APPEND _sdoj_vc_crt_dirs "${_sdoj_vc_redist_root}")
        endif()
        file(GLOB _sdoj_vc_crt_candidates LIST_DIRECTORIES TRUE
            "${_sdoj_vc_redist_root}/Microsoft.VC*.CRT"
            "${_sdoj_vc_redist_root}/${_sdoj_vc_redist_arch}/Microsoft.VC*.CRT"
            "${_sdoj_vc_redist_root}/*/${_sdoj_vc_redist_arch}/Microsoft.VC*.CRT"
        )
        list(APPEND _sdoj_vc_crt_dirs ${_sdoj_vc_crt_candidates})
        list(REMOVE_DUPLICATES _sdoj_vc_crt_dirs)
        list(SORT _sdoj_vc_crt_dirs COMPARE NATURAL ORDER DESCENDING)
    endif()

    set(_sdoj_vc_crt_files
        msvcp140.dll
        msvcp140_atomic_wait.dll
        vcruntime140.dll
        vcruntime140_1.dll
    )
    set(_sdoj_vc_crt_dir "")
    foreach(_sdoj_candidate_dir IN LISTS _sdoj_vc_crt_dirs)
        set(_sdoj_candidate_complete TRUE)
        foreach(_sdoj_crt_file IN LISTS _sdoj_vc_crt_files)
            if(NOT EXISTS "${_sdoj_candidate_dir}/${_sdoj_crt_file}")
                set(_sdoj_candidate_complete FALSE)
                break()
            endif()
        endforeach()
        if(_sdoj_candidate_complete)
            set(_sdoj_vc_crt_dir "${_sdoj_candidate_dir}")
            break()
        endif()
    endforeach()

    if(NOT _sdoj_vc_crt_dir)
        message(FATAL_ERROR
            "Could not find all app-local Visual C++ runtime DLLs "
            "(${_sdoj_vc_crt_files}). Set SDOJ_VC_REDIST_DIR to the "
            "${_sdoj_vc_redist_arch}/Microsoft.VC*.CRT redist directory, or "
            "configure from a Visual Studio developer environment that sets VCToolsRedistDir.")
    endif()

    message(STATUS "Using app-local Visual C++ runtime from: ${_sdoj_vc_crt_dir}")
    foreach(_sdoj_crt_file IN LISTS _sdoj_vc_crt_files)
        add_custom_command(TARGET saidaioujou_recomp_tu1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_sdoj_vc_crt_dir}/${_sdoj_crt_file}"
                "$<TARGET_FILE_DIR:saidaioujou_recomp_tu1>/${_sdoj_crt_file}"
            VERBATIM
        )
    endforeach()

    target_link_options(saidaioujou_recomp_tu1_CA022100 PRIVATE "LINKER:/MAP")
    target_link_libraries(saidaioujou_recomp_tu1 PRIVATE user32 imm32 winhttp shell32)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    find_package(CURL REQUIRED)
    target_include_directories(saidaioujou_recomp_tu1 PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(saidaioujou_recomp_tu1 PRIVATE ${GTK3_LIBRARIES} CURL::libcurl)

    # copy the runtime libraries next to the executable
    set(_sdoj_runtime_libs $<TARGET_FILE:rexruntime>)
    if(TARGET TracyClient)
        list(APPEND _sdoj_runtime_libs $<TARGET_FILE:TracyClient>)
    endif()
    add_custom_command(TARGET saidaioujou_recomp_tu1 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_sdoj_runtime_libs}
            "$<TARGET_FILE_DIR:saidaioujou_recomp_tu1>"
        VERBATIM
    )
endif()
