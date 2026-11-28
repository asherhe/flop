set(flop_source_dir "${PROJECT_SOURCE_DIR}/src/flop")
# use the production shader toolchain for probe fixtures
get_directory_property(dxc DIRECTORY "${FLOP_BUILD_ASSETS_DIR}" DEFINITION dxc)
get_directory_property(FLOP_SHADER_FORMATS DIRECTORY "${FLOP_BUILD_ASSETS_DIR}" DEFINITION FLOP_SHADER_FORMATS)
get_directory_property(shader_source_dir DIRECTORY "${FLOP_BUILD_ASSETS_DIR}" DEFINITION shader_source_dir)
get_directory_property(shader_includes DIRECTORY "${FLOP_BUILD_ASSETS_DIR}" DEFINITION shader_includes)

set(gpu_test_shaders)
add_executable(flop_input_probe input_probe.cpp
    "${flop_source_dir}/platform/input_mapper.cpp"
    "${flop_source_dir}/platform/sdl_platform.cpp")
target_link_libraries(flop_input_probe PRIVATE flop_core SDL3::SDL3)
if(WIN32)
    add_custom_command(TARGET flop_input_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_input_probe>" VERBATIM)
endif()

foreach(stage IN ITEMS vertex fragment)
    if(stage STREQUAL vertex)
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/vertex.hlsl")
        set(profile vs_6_0)
    else()
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/fragment.hlsl")
        set(profile ps_6_0)
    endif()
    foreach(format IN LISTS FLOP_SHADER_FORMATS)
        set(flags)
        if(format STREQUAL spirv)
            list(APPEND flags -spirv -fspv-target-env=vulkan1.0)
        endif()
        set(output "${CMAKE_CURRENT_BINARY_DIR}/${stage}.${format}")
        add_custom_command(OUTPUT "${output}"
            COMMAND "${dxc}" -T "${profile}" -E "${stage}" ${flags} -Fo "${output}" "${source}"
            DEPENDS "${source}" "${dxc}" VERBATIM)
        list(APPEND gpu_test_shaders "${output}")
    endforeach()
endforeach()
add_executable(flop_gpu_probe sampling_probe.cpp ${gpu_test_shaders})
target_link_libraries(flop_gpu_probe PRIVATE SDL3::SDL3)
if("dxil" IN_LIST FLOP_SHADER_FORMATS)
    target_compile_definitions(flop_gpu_probe PRIVATE FLOP_PROBE_DXIL)
endif()
target_compile_definitions(flop_gpu_probe PRIVATE FLOP_GPU_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}")
if(WIN32)
    add_custom_command(TARGET flop_gpu_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_gpu_probe>" VERBATIM)
endif()

get_property(registered_shaders GLOBAL PROPERTY FLOP_SHADER_IDS)
if("craft/sprite" IN_LIST registered_shaders)
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
endif()

set(craft_probe_shaders)
foreach(stage IN ITEMS vertex fragment)
    if(stage STREQUAL vertex)
        set(profile vs_6_0)
    else()
        set(profile ps_6_0)
    endif()
    foreach(format IN LISTS FLOP_SHADER_FORMATS)
        set(flags)
        if(format STREQUAL spirv)
            list(APPEND flags -spirv -fspv-target-env=vulkan1.0)
        endif()
        set(output "${CMAKE_CURRENT_BINARY_DIR}/craft/${stage}.${format}")
        set(source "${shader_source_dir}/craft/sprite.hlsl")
        add_custom_command(OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/craft"
            COMMAND "${dxc}" -T "${profile}" -E "${stage}" ${flags} -Fo "${output}" "${source}"
            DEPENDS "${source}" "${dxc}" ${shader_includes} VERBATIM)
        list(APPEND craft_probe_shaders "${output}")
    endforeach()
endforeach()
add_executable(flop_craft_probe craft_probe.cpp ${craft_probe_shaders})
target_link_libraries(flop_craft_probe PRIVATE SDL3::SDL3)
if("dxil" IN_LIST FLOP_SHADER_FORMATS)
    target_compile_definitions(flop_craft_probe PRIVATE FLOP_PROBE_DXIL)
endif()
target_compile_definitions(flop_craft_probe PRIVATE FLOP_CRAFT_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/craft")
if(WIN32)
    add_custom_command(TARGET flop_craft_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_craft_probe>" VERBATIM)
endif()

add_executable(flop_invalid_png_probe invalid_png_probe.cpp "${flop_source_dir}/render/texture_cache.cpp")
target_link_libraries(flop_invalid_png_probe PRIVATE flop_core SDL3::SDL3)
target_include_directories(flop_invalid_png_probe SYSTEM PRIVATE "${stb_SOURCE_DIR}")
if(WIN32)
    add_custom_command(TARGET flop_invalid_png_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_invalid_png_probe>" VERBATIM)
endif()
