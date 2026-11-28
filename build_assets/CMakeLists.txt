include(shaders)
set(smaa_header "${shader_dir}/smaa_lookup.hpp")
set(smaa_source_dir "${FLOP_BUILD_ASSETS_DIR}/resources/smaa")
set(smaa_script "${PROJECT_SOURCE_DIR}/cmake/embed_smaa.cmake")
add_custom_command(OUTPUT "${smaa_header}"
    COMMAND "${CMAKE_COMMAND}" "-DOUTPUT=${smaa_header}"
        "-DINPUT_DIR=${smaa_source_dir}"
        -P "${smaa_script}"
    DEPENDS "${smaa_source_dir}/area.png" "${smaa_source_dir}/search.png"
        "${smaa_script}"
    VERBATIM)
add_custom_target(flop_smaa_lookup DEPENDS "${smaa_header}")

add_subdirectory(shaders)

add_library(flop_shader_assets INTERFACE)
target_include_directories(flop_shader_assets INTERFACE "${shader_dir}")
add_dependencies(flop_shader_assets flop_shader_package flop_smaa_lookup)
