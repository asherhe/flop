if(NOT COMMAND catch_discover_tests AND DEFINED catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()
include(Catch)

add_executable(flop_core_tests
    craft/actuator_tests.cpp
    craft/control_tests.cpp
    craft/craft_tests.cpp
    craft/headless_simulation_tests.cpp
    craft/ids_tests.cpp
    craft/newtonian_tests.cpp
    craft/part_plugin_tests.cpp
    craft/particle_rgb_tests.cpp
    craft/resources_tests.cpp
    craft/resource_registry_tests.cpp
    math/vec2_tests.cpp
    math/key_curve_tests.cpp
    math/noise_tests.cpp
    orbit/encounter_tests.cpp
    orbit/patched_conics_tests.cpp
    orbit/propagation_tests.cpp
    orbit/reference_frame_tests.cpp
    orbit/state_tests.cpp
    world/celestial_body_tests.cpp
    world/control_manager_tests.cpp
    world/craft_ownership_tests.cpp
    world/universe_tests.cpp
    world/universe_tick_tests.cpp
)
target_link_libraries(flop_core_tests PRIVATE flop_core Catch2::Catch2WithMain)
target_include_directories(flop_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
catch_discover_tests(flop_core_tests PROPERTIES LABELS core)

add_executable(flop_app_tests
    "${PROJECT_SOURCE_DIR}/src/flop/render/material_recipe.cpp"
    app/game_scene_tests.cpp
    app/frame_tests.cpp
    app/particle_tests.cpp
    app/tick_scheduler_tests.cpp
    config/body_decode_tests.cpp
    config/body_orbit_decode_tests.cpp
    config/body_visual_decode_tests.cpp
    config/craft_decode_tests.cpp
    config/part_decode_tests.cpp
    config/resources_decode_tests.cpp
    config/save_decode_tests.cpp
    config/shared_decode_tests.cpp
    config/config_tests.cpp
    render/shader_library_tests.cpp
    render/material_recipe_tests.cpp
)
target_link_libraries(flop_app_tests PRIVATE flop_core Catch2::Catch2WithMain)
target_include_directories(flop_app_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(flop_app_tests PRIVATE
    FLOP_TEST_ASSET_ROOT="${FLOP_ASSETS_DIR}")
catch_discover_tests(flop_app_tests PROPERTIES LABELS app)
