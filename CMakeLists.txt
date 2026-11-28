cmake_minimum_required(VERSION 3.25)
project(flop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FLOP_BUILD_GAME "Build the desktop game" OFF)

set(FLOP_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(FLOP_BUILD_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build_assets")
set(FLOP_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(CTest)
include(dependencies)

if(FLOP_BUILD_GAME)
    add_subdirectory("${FLOP_BUILD_ASSETS_DIR}" generated)
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(FLOP_BUILD_GAME)
    add_subdirectory(tools/gpu_probe EXCLUDE_FROM_ALL)
endif()
