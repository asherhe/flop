set(flop_source_dir "${PROJECT_SOURCE_DIR}/src/flop")

add_executable(flop_input_probe input_probe.cpp
    "${flop_source_dir}/platform/input_mapper.cpp"
    "${flop_source_dir}/platform/sdl_platform.cpp")
target_link_libraries(flop_input_probe PRIVATE flop_core SDL3::SDL3)
if(WIN32)
    add_custom_command(TARGET flop_input_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_input_probe>" VERBATIM)
endif()

add_executable(flop_renderer_probe renderer_probe.cpp
    "${flop_source_dir}/render/renderer.cpp"
    "${flop_source_dir}/render/material_recipe.cpp"
    "${flop_source_dir}/render/texture_cache.cpp")
target_link_libraries(flop_renderer_probe PRIVATE flop_core SDL3::SDL3 flop_shader_assets)
target_include_directories(flop_renderer_probe SYSTEM PRIVATE "${stb_SOURCE_DIR}")
target_compile_definitions(flop_renderer_probe PRIVATE FLOP_RENDER_TESTING)
if(WIN32)
    add_custom_command(TARGET flop_renderer_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_renderer_probe>" VERBATIM)
endif()

add_executable(flop_scene_lighting_probe scene_lighting_probe.cpp
    "${flop_source_dir}/render/renderer.cpp"
    "${flop_source_dir}/render/material_recipe.cpp"
    "${flop_source_dir}/render/texture_cache.cpp")
target_link_libraries(flop_scene_lighting_probe PRIVATE flop_core SDL3::SDL3 flop_shader_assets)
target_include_directories(flop_scene_lighting_probe SYSTEM PRIVATE "${stb_SOURCE_DIR}")
target_compile_definitions(flop_scene_lighting_probe PRIVATE FLOP_RENDER_TESTING)
if(WIN32)
    add_custom_command(TARGET flop_scene_lighting_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_scene_lighting_probe>" VERBATIM)
endif()
