if(BUILD_TESTING)
  set(shader_test_dir "${PROJECT_SOURCE_DIR}/tests/render")
  add_test(NAME shader_reflection_fixtures
    COMMAND "${CMAKE_COMMAND}" "-DDXC=${dxc}" "-DREFLECT=$<TARGET_FILE:flop_spirv_reflect>"
      "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DFIXTURE_DIR=${shader_test_dir}/fixtures"
      "-DEMBED=${CMAKE_CURRENT_LIST_DIR}/embed_shaders.cmake"
      "-DTEST_DXIL=$<BOOL:$<TARGET_EXISTS:flop_dxil_reflect>>"
      -P "${shader_test_dir}/reflection_test.cmake")
  set_tests_properties(shader_reflection_fixtures PROPERTIES LABELS "app;shader")
  if(TARGET flop_dxil_reflect)
    add_test(NAME dxil_reflection_fixture
      COMMAND "${CMAKE_COMMAND}" "-DDXC=${dxc}" "-DREFLECT=$<TARGET_FILE:flop_dxil_reflect>"
        "-DSPIRV_REFLECT=$<TARGET_FILE:flop_spirv_reflect>"
        "-DCOMPARE=${CMAKE_CURRENT_LIST_DIR}/compare_shader_reflection.cmake"
        "-DEMBED=${CMAKE_CURRENT_LIST_DIR}/embed_shaders.cmake"
        "-DDXCOMPILER=${dxcompiler}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DFIXTURE_DIR=${shader_test_dir}/fixtures"
        -P "${shader_test_dir}/dxil_reflection_test.cmake")
    set_tests_properties(dxil_reflection_fixture PROPERTIES LABELS "app;shader")
  endif()
endif()
set(shader_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(shader_source_dir "${FLOP_BUILD_ASSETS_DIR}/shaders")
file(MAKE_DIRECTORY "${shader_dir}")
# Recompile on shared include changes, including when an include is added or removed.
file(GLOB_RECURSE shader_includes CONFIGURE_DEPENDS
     "${shader_source_dir}/*.hlsli")

function(flop_compile_shader source output profile entry format)
  set(flags)
  if(format STREQUAL spirv)
    list(APPEND flags -spirv -fspv-target-env=vulkan1.0)
  endif()
  add_custom_command(OUTPUT "${output}"
    COMMAND "${dxc}" -T "${profile}" -E "${entry}" ${flags} -Fo "${output}" "${source}"
    DEPENDS "${source}" "${dxc}" ${ARGN}
    VERBATIM)
endfunction()

function(flop_add_shader)
  set(shader_script_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
  cmake_parse_arguments(PARSE_ARGV 0 SHADER "RESOURCE_FREE" "SOURCE;VERTEX_ENTRY;FRAGMENT_ENTRY" "")
  if(SHADER_UNPARSED_ARGUMENTS OR SHADER_KEYWORDS_MISSING_VALUES OR NOT SHADER_SOURCE)
    message(FATAL_ERROR "flop_add_shader requires SOURCE; optional arguments are RESOURCE_FREE, VERTEX_ENTRY and FRAGMENT_ENTRY")
  endif()
  if(NOT SHADER_SOURCE MATCHES "^[A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)*[.]hlsl$")
    message(FATAL_ERROR "Invalid shader source path: '${SHADER_SOURCE}'")
  endif()
  string(REGEX REPLACE "[.]hlsl$" "" shader_stem "${SHADER_SOURCE}")
  set(SHADER_ID "shader:${shader_stem}")
  get_property(ids GLOBAL PROPERTY FLOP_SHADER_IDS)
  if(SHADER_ID IN_LIST ids)
    message(FATAL_ERROR "Duplicate shader ID '${SHADER_ID}'")
  endif()
  set_property(GLOBAL APPEND PROPERTY FLOP_SHADER_IDS "${SHADER_ID}")
  set(shader_source "${shader_source_dir}/${SHADER_SOURCE}")
  if(NOT EXISTS "${shader_source}" OR IS_DIRECTORY "${shader_source}")
    message(FATAL_ERROR "Shader '${SHADER_ID}' source not found: ${shader_source}")
  endif()
  if(NOT SHADER_VERTEX_ENTRY)
    set(SHADER_VERTEX_ENTRY vertex)
  endif()
  if(NOT SHADER_FRAGMENT_ENTRY)
    set(SHADER_FRAGMENT_ENTRY fragment)
  endif()
  foreach(stage IN ITEMS vertex fragment)
    if(stage STREQUAL vertex)
      set(profile vs_6_0)
      set(entry "${SHADER_VERTEX_ENTRY}")
    else()
      set(profile ps_6_0)
      set(entry "${SHADER_FRAGMENT_ENTRY}")
    endif()
    if(NOT entry MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
      message(FATAL_ERROR "Invalid ${stage} entry point '${entry}' for shader '${SHADER_ID}'")
    endif()
    set(reflection "${shader_dir}/${shader_stem}.${stage}.reflection.json")
    if(TARGET flop_dxil_reflect)
      set(dxil_reflection "${shader_dir}/${shader_stem}.${stage}.dxil.reflection.json")
    else()
      set(dxil_reflection "none")
    endif()
    foreach(format IN LISTS FLOP_SHADER_FORMATS)
      set(output "${shader_dir}/${shader_stem}.${stage}.${format}")
      get_filename_component(output_dir "${output}" DIRECTORY)
      file(MAKE_DIRECTORY "${output_dir}")
      flop_compile_shader("${shader_source}" "${output}" "${profile}" "${entry}" "${format}" ${shader_includes})
      set_property(GLOBAL APPEND PROPERTY FLOP_SHADER_OUTPUTS "${output}")
      set_property(GLOBAL APPEND_STRING PROPERTY FLOP_SHADER_MANIFEST
        "${SHADER_ID}|${stage}|${format}|${entry}|${output}|${reflection}|${SHADER_RESOURCE_FREE}|${dxil_reflection}\n")
    endforeach()
    set(spirv_file "${shader_dir}/${shader_stem}.${stage}.spirv")
    add_custom_command(OUTPUT "${reflection}"
      COMMAND "${CMAKE_COMMAND}" "-DREFLECT=$<TARGET_FILE:flop_spirv_reflect>"
        "-DSHADER=${spirv_file}" "-DOUTPUT=${reflection}" "-DID=${SHADER_ID}"
        "-DSTAGE=${stage}" "-DENTRY=${entry}" -P "${shader_script_dir}/reflect_shader.cmake"
      DEPENDS "${spirv_file}" flop_spirv_reflect "${shader_script_dir}/reflect_shader.cmake" VERBATIM)
    set_property(GLOBAL APPEND PROPERTY FLOP_SHADER_OUTPUTS "${reflection}")
    if(TARGET flop_dxil_reflect)
      set(dxil_file "${shader_dir}/${shader_stem}.${stage}.dxil")
      add_custom_command(OUTPUT "${dxil_reflection}"
        COMMAND "${CMAKE_COMMAND}" "-DREFLECT=$<TARGET_FILE:flop_dxil_reflect>"
          "-DDXCOMPILER=${dxcompiler}"
          "-DSHADER=${dxil_file}" "-DOUTPUT=${dxil_reflection}" "-DID=${SHADER_ID}"
          "-DSTAGE=${stage}" -P "${shader_script_dir}/reflect_dxil.cmake"
        DEPENDS "${dxil_file}" "${dxcompiler}" flop_dxil_reflect "${shader_script_dir}/reflect_dxil.cmake" VERBATIM)
      set(comparison "${shader_dir}/${shader_stem}.${stage}.validated")
      add_custom_command(OUTPUT "${comparison}"
        COMMAND "${CMAKE_COMMAND}" "-DSPIRV=${reflection}" "-DDXIL=${dxil_reflection}"
          "-DCOMPARE=${shader_script_dir}/compare_shader_reflection.cmake"
          "-DOUTPUT=${comparison}" "-DID=${SHADER_ID}" "-DSTAGE=${stage}"
          -P "${shader_script_dir}/validate_shader.cmake"
        DEPENDS "${reflection}" "${dxil_reflection}" "${shader_script_dir}/compare_shader_reflection.cmake"
          "${shader_script_dir}/validate_shader.cmake" VERBATIM)
      set_property(GLOBAL APPEND PROPERTY FLOP_SHADER_OUTPUTS "${dxil_reflection}" "${comparison}")
    endif()
  endforeach()
endfunction()

function(flop_embed_shaders target)
  get_property(shader_outputs GLOBAL PROPERTY FLOP_SHADER_OUTPUTS)
  get_property(shader_manifest_content GLOBAL PROPERTY FLOP_SHADER_MANIFEST)
  if(NOT shader_outputs)
    message(FATAL_ERROR "No shaders registered for ${target}")
  endif()
  set(shader_manifest "${shader_dir}/manifest.txt")
  file(CONFIGURE OUTPUT "${shader_manifest}" CONTENT "${shader_manifest_content}" @ONLY)
  set(shader_header "${shader_dir}/embedded.hpp")
  set(embed_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embed_shaders.cmake")
  add_custom_command(OUTPUT "${shader_header}"
    COMMAND "${CMAKE_COMMAND}" "-DOUTPUT=${shader_header}" "-DINPUT=${shader_manifest}" -P "${embed_script}"
    DEPENDS ${shader_outputs} "${shader_manifest}" "${embed_script}" VERBATIM)
  add_custom_target(${target} DEPENDS "${shader_header}")
  if(BUILD_TESTING)
    add_test(NAME craft_shader_package
      COMMAND "${CMAKE_COMMAND}" "-DINPUT=${shader_manifest}"
        "-DEXPECT_DXIL=$<BOOL:$<TARGET_EXISTS:flop_dxil_reflect>>"
        -P "${shader_test_dir}/package_test.cmake")
    set_tests_properties(craft_shader_package PROPERTIES LABELS "app;shader")
  endif()
endfunction()
