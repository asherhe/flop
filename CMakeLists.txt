cmake_minimum_required(VERSION 3.31.8)
project(flop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FLOP_BUILD_GAME "Build the desktop game" OFF)
if(FLOP_BUILD_GAME)
    message(FATAL_ERROR
        "FLOP_BUILD_GAME requires the desktop game target. Use OFF for the headless build.")
endif()

add_library(flop_core STATIC
    src/flop/orbit/ref_frame.cpp
    src/flop/orbit/state.cpp
    src/flop/orbit/propagators/universal.cpp
)
target_include_directories(flop_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(flop_core PUBLIC cxx_std_20)

include(CTest)
if(BUILD_TESTING)
    include(cmake/dependencies.cmake)

    add_executable(flop_tests
        tests/math/vec2_tests.cpp
        tests/orbit/propagation_tests.cpp
        tests/orbit/reference_frame_tests.cpp
    )
    target_link_libraries(flop_tests PRIVATE flop_core Catch2::Catch2WithMain)

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(flop_tests)
endif()

