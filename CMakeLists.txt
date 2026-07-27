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
