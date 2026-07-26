# saidaioujou_recomp_tu1 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(saidaioujou_recomp_tu1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk"
    CACHE PATH "Path to the bundled ReXGlue SDK")

include(generated/rexglue.cmake)

# Sources
set(SAIDAIOUJOURECOMPTU1_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(saidaioujou_recomp_tu1 WIN32 ${SAIDAIOUJOURECOMPTU1_SOURCES})
else()
    add_executable(saidaioujou_recomp_tu1 ${SAIDAIOUJOURECOMPTU1_SOURCES})
endif()

rexglue_setup_target(saidaioujou_recomp_tu1)

if(TARGET rexgpu-xenos)
    add_dependencies(saidaioujou_recomp_tu1 rexgpu-xenos)
    add_custom_target(stage_rexgpu_xenos ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:rexgpu-xenos>
            $<TARGET_FILE_DIR:saidaioujou_recomp_tu1>
        DEPENDS rexgpu-xenos)
endif()
