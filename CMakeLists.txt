cmake_minimum_required(VERSION 3.25)
project(flop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FLOP_BUILD_GAME "Build the desktop game" OFF)

add_library(flop_core STATIC
    src/flop/app/game_scene.cpp
    src/flop/app/tick_scheduler.cpp
    src/flop/craft/craft.cpp
    src/flop/craft/newtonian.cpp
    src/flop/craft/resources.cpp
    src/flop/craft/resource_registry.cpp
    src/flop/craft/parts/part.cpp
    src/flop/craft/plugins/plugin_factory.cpp
    src/flop/craft/plugins/engine_plugin.cpp
    src/flop/craft/plugins/resource_plugin.cpp
    src/flop/config/asset_path.cpp
    src/flop/config/load.cpp
    src/flop/orbit/ref_frame.cpp
    src/flop/orbit/state.cpp
    src/flop/orbit/prediction/encounter.cpp
    src/flop/orbit/prediction/patched_conics.cpp
    src/flop/orbit/prediction/soi_escape.cpp
    src/flop/orbit/prediction/soi_intercept.cpp
    src/flop/orbit/propagators/universal.cpp
    src/flop/view/frame.cpp
    src/flop/world/control_manager.cpp
    src/flop/world/celestial_body.cpp
    src/flop/world/universe.cpp
)
target_include_directories(flop_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(flop_core PUBLIC cxx_std_20)

include(CTest)
include(cmake/dependencies.cmake)
target_link_libraries(flop_core PUBLIC yaml-cpp::yaml-cpp)

if(BUILD_TESTING)
    if(NOT COMMAND catch_discover_tests AND DEFINED catch2_SOURCE_DIR)
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
    include(Catch)

    add_executable(flop_core_tests
        tests/craft/actuator_tests.cpp
        tests/craft/control_tests.cpp
        tests/craft/craft_tests.cpp
        tests/craft/headless_simulation_tests.cpp
        tests/craft/ids_tests.cpp
        tests/craft/newtonian_tests.cpp
        tests/craft/part_plugin_tests.cpp
        tests/craft/resources_tests.cpp
        tests/craft/resource_registry_tests.cpp
        tests/math/vec2_tests.cpp
        tests/orbit/encounter_tests.cpp
        tests/orbit/patched_conics_tests.cpp
        tests/orbit/propagation_tests.cpp
        tests/orbit/reference_frame_tests.cpp
        tests/orbit/state_tests.cpp
        tests/world/celestial_body_tests.cpp
        tests/world/control_manager_tests.cpp
        tests/world/craft_ownership_tests.cpp
        tests/world/universe_tests.cpp
        tests/world/universe_tick_tests.cpp
    )
    target_link_libraries(flop_core_tests PRIVATE flop_core Catch2::Catch2WithMain)
    target_include_directories(flop_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    catch_discover_tests(flop_core_tests PROPERTIES LABELS core)

    add_executable(flop_app_tests
        tests/app/game_scene_tests.cpp
        tests/app/frame_tests.cpp
        tests/app/tick_scheduler_tests.cpp
        tests/config/body_decode_tests.cpp
        tests/config/body_orbit_decode_tests.cpp
        tests/config/body_visual_decode_tests.cpp
        tests/config/craft_decode_tests.cpp
        tests/config/part_decode_tests.cpp
        tests/config/resources_decode_tests.cpp
        tests/config/save_decode_tests.cpp
        tests/config/config_tests.cpp
        tests/render/shader_library_tests.cpp
    )
    target_link_libraries(flop_app_tests PRIVATE flop_core Catch2::Catch2WithMain)
    target_include_directories(flop_app_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_definitions(flop_app_tests PRIVATE
        FLOP_TEST_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    catch_discover_tests(flop_app_tests PROPERTIES LABELS app)
endif()

if(FLOP_BUILD_GAME)
    add_executable(flop
        src/flop/app/app.cpp
        src/flop/app/main.cpp
        src/flop/input/pilot_control.cpp
        src/flop/platform/input_mapper.cpp
        src/flop/platform/sdl_platform.cpp
        src/flop/render/renderer.cpp
    )

    target_link_libraries(flop PRIVATE flop_core SDL3::SDL3)
    include(cmake/shaders.cmake)
    flop_add_shader(ID test_tri SOURCE test_tri.hlsl)
    flop_embed_shaders(flop)
    add_subdirectory(tools/gpu_probe EXCLUDE_FROM_ALL)

    # package SDL3.dll alongside flop.exe on Windows.
    if(WIN32)
        add_custom_command(TARGET flop POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3-shared>"
                "$<TARGET_FILE_DIR:flop>"
            VERBATIM
        )
    endif()
endif()


