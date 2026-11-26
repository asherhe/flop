set(gpu_test_shaders)
add_executable(flop_input_probe input_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/flop/platform/input_mapper.cpp
    ${CMAKE_SOURCE_DIR}/src/flop/platform/sdl_platform.cpp)
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
        ${CMAKE_SOURCE_DIR}/src/flop/render/renderer.cpp
        ${CMAKE_SOURCE_DIR}/src/flop/render/material_recipe.cpp
        ${CMAKE_SOURCE_DIR}/src/flop/render/texture_cache.cpp)
    add_dependencies(flop_renderer_probe flop)
    target_link_libraries(flop_renderer_probe PRIVATE flop_core SDL3::SDL3)
    target_include_directories(flop_renderer_probe PRIVATE "${shader_dir}")
    target_include_directories(flop_renderer_probe SYSTEM PRIVATE "${stb_SOURCE_DIR}")
    target_compile_definitions(flop_renderer_probe PRIVATE FLOP_RENDER_TESTING)
    if(WIN32)
        add_custom_command(TARGET flop_renderer_probe POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_renderer_probe>" VERBATIM)
    endif()

    add_executable(flop_scene_lighting_probe scene_lighting_probe.cpp
        ${CMAKE_SOURCE_DIR}/src/flop/render/renderer.cpp
        ${CMAKE_SOURCE_DIR}/src/flop/render/material_recipe.cpp
        ${CMAKE_SOURCE_DIR}/src/flop/render/texture_cache.cpp)
    add_dependencies(flop_scene_lighting_probe flop)
    target_link_libraries(flop_scene_lighting_probe PRIVATE flop_core SDL3::SDL3)
    target_include_directories(flop_scene_lighting_probe PRIVATE "${shader_dir}")
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
        set(source "${CMAKE_SOURCE_DIR}/src/flop/render/shaders/craft/sprite.hlsl")
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

add_executable(flop_invalid_png_probe invalid_png_probe.cpp ${CMAKE_SOURCE_DIR}/src/flop/render/texture_cache.cpp)
target_link_libraries(flop_invalid_png_probe PRIVATE flop_core SDL3::SDL3)
target_include_directories(flop_invalid_png_probe SYSTEM PRIVATE "${stb_SOURCE_DIR}")
if(WIN32)
    add_custom_command(TARGET flop_invalid_png_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:flop_invalid_png_probe>" VERBATIM)
endif()
