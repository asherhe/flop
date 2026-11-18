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
    src/flop/craft/craft.cpp
    src/flop/craft/resource_registry.cpp
    src/flop/config/asset_path.cpp
    src/flop/config/decode/body.cpp
    src/flop/config/decode/body_orbit.cpp
    src/flop/config/decode/craft.cpp
    src/flop/config/decode/resources.cpp
    src/flop/config/load.cpp
    src/flop/orbit/ref_frame.cpp
    src/flop/orbit/state.cpp
    src/flop/orbit/prediction/encounter.cpp
    src/flop/orbit/prediction/patched_conics.cpp
    src/flop/orbit/prediction/soi_escape.cpp
    src/flop/orbit/prediction/soi_intercept.cpp
    src/flop/orbit/propagators/universal.cpp
    src/flop/world/celestial_body.cpp
    src/flop/world/universe.cpp
)
target_include_directories(flop_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(flop_core PUBLIC cxx_std_20)

include(CTest)
include(cmake/dependencies.cmake)
target_link_libraries(flop_core PUBLIC yaml-cpp::yaml-cpp)

if(BUILD_TESTING)
    add_executable(flop_tests
        tests/craft/craft_tests.cpp
        tests/craft/resource_registry_tests.cpp
        tests/math/vec2_tests.cpp
        tests/orbit/encounter_tests.cpp
        tests/orbit/patched_conics_tests.cpp
        tests/orbit/propagation_tests.cpp
        tests/orbit/reference_frame_tests.cpp
        tests/world/celestial_body_tests.cpp
        tests/world/universe_tests.cpp
    )
    target_link_libraries(flop_tests PRIVATE flop_core Catch2::Catch2WithMain)

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(flop_tests)

    add_executable(flop_config_tests
        tests/config/body_decode_tests.cpp
        tests/config/body_orbit_decode_tests.cpp
        tests/config/craft_decode_tests.cpp
        tests/config/resources_decode_tests.cpp
        tests/config/config_tests.cpp
    )
    target_link_libraries(flop_config_tests PRIVATE flop_core Catch2::Catch2WithMain)
    target_compile_definitions(flop_config_tests PRIVATE
        FLOP_TEST_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    catch_discover_tests(flop_config_tests)
endif()

