# Microsoft DXC v1.8.2505; release contains LLVM, MIT and Microsoft license files.
# Hashes are published by SDL_shadercross's download-prebuilt-DirectXShaderCompiler.cmake.
include(FetchContent)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "AMD64|amd64|x86_64")
  set(dxc_url https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505/dxc_2025_05_24.zip)
  set(dxc_hash 81380f3eca156d902d6404fd6df9f4b0886f576ff3e18b2cc10d3075ffc9d119)
  set(dxc_relative bin/x64/dxc.exe)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  set(dxc_url https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505/linux_dxc_2025_05_24.x86_64.tar.gz)
  set(dxc_hash b99655f65215287825fcdd49102b17e2a1608eff79ffaf9457514c2676892aa5)
  set(dxc_relative bin/dxc)
else()
  message(FATAL_ERROR "No pinned DXC host package for ${CMAKE_HOST_SYSTEM_NAME}/${CMAKE_HOST_SYSTEM_PROCESSOR}; Metal shader generation remains unverified")
endif()
FetchContent_Declare(flop_dxc URL "${dxc_url}" URL_HASH "SHA256=${dxc_hash}")
FetchContent_MakeAvailable(flop_dxc)
set(dxc "${flop_dxc_SOURCE_DIR}/${dxc_relative}")

set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(flop_spirv_cross
  URL https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/vulkan-sdk-1.4.304.0.tar.gz
  URL_HASH SHA256=635b9b9ed2318df5ac65a0e1db1f92deb1e9c29e9dac30cd4b14eb3d72be5cf3)
FetchContent_MakeAvailable(flop_spirv_cross)
add_executable(flop_spirv_reflect tools/reflection/spirv_reflect.cpp)
target_link_libraries(flop_spirv_reflect PRIVATE spirv-cross-core)
if(CMAKE_HOST_WIN32 AND WIN32)
  add_executable(flop_dxil_reflect tools/reflection/dxil_reflect.cpp)
  target_include_directories(flop_dxil_reflect SYSTEM PRIVATE "${flop_dxc_SOURCE_DIR}/inc")
endif()
if(BUILD_TESTING)
  add_test(NAME shader_reflection_fixtures
    COMMAND "${CMAKE_COMMAND}" "-DDXC=${dxc}" "-DREFLECT=$<TARGET_FILE:flop_spirv_reflect>"
      "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/render/fixtures"
      "-DEMBED=${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/reflection_test.cmake")
  set_tests_properties(shader_reflection_fixtures PROPERTIES LABELS "app;shader")
  if(TARGET flop_dxil_reflect)
    add_test(NAME dxil_reflection_fixture
      COMMAND "${CMAKE_COMMAND}" "-DDXC=${dxc}" "-DREFLECT=$<TARGET_FILE:flop_dxil_reflect>"
        "-DSPIRV_REFLECT=$<TARGET_FILE:flop_spirv_reflect>"
        "-DCOMPARE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/compare_shader_reflection.cmake"
        "-DEMBED=${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake"
        "-DDXCOMPILER=${flop_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/render/fixtures"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/dxil_reflection_test.cmake")
    set_tests_properties(dxil_reflection_fixture PROPERTIES LABELS "app;shader")
  endif()
endif()
set(shader_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
file(MAKE_DIRECTORY "${shader_dir}")
# Recompile on shared include changes, including when an include is added or removed.
file(GLOB_RECURSE shader_includes CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/flop/render/shaders/*.hlsli")

# register a shader
#   ID - identifier for shader
#   SOURCE is relative to the shader directory
function(flop_add_shader)
  cmake_parse_arguments(SHADER "RESOURCE_FREE" "ID;SOURCE;VERTEX_ENTRY;FRAGMENT_ENTRY" "" ${ARGN})
  if(SHADER_UNPARSED_ARGUMENTS OR NOT SHADER_ID OR NOT SHADER_SOURCE)
    message(FATAL_ERROR "flop_add_shader requires ID and SOURCE; optional arguments are RESOURCE_FREE, VERTEX_ENTRY and FRAGMENT_ENTRY")
  endif()
  if(NOT SHADER_ID MATCHES "^[A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)*$")
    message(FATAL_ERROR "Invalid shader ID '${SHADER_ID}': use slash-separated letters, digits, underscores or hyphens")
  endif()
  get_property(ids GLOBAL PROPERTY FLOP_SHADER_IDS)
  if(SHADER_ID IN_LIST ids)
    message(FATAL_ERROR "Duplicate shader ID '${SHADER_ID}'")
  endif()
  set_property(GLOBAL APPEND PROPERTY FLOP_SHADER_IDS "${SHADER_ID}")
  set(shader_source "${CMAKE_CURRENT_SOURCE_DIR}/src/flop/render/shaders/${SHADER_SOURCE}")
  if(NOT EXISTS "${shader_source}")
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
    set(reflection "${shader_dir}/${SHADER_ID}.${stage}.reflection.json")
    if(TARGET flop_dxil_reflect)
      set(dxil_reflection "${shader_dir}/${SHADER_ID}.${stage}.dxil.reflection.json")
    else()
      set(dxil_reflection "none")
    endif()
    foreach(format IN ITEMS spirv dxil)
      set(output "${shader_dir}/${SHADER_ID}.${stage}.${format}")
      get_filename_component(output_dir "${output}" DIRECTORY)
      file(MAKE_DIRECTORY "${output_dir}")
      set(flags)
      if("${format}" STREQUAL "spirv")
        list(APPEND flags -spirv -fspv-target-env=vulkan1.0)
      endif()
      add_custom_command(OUTPUT "${output}"
        COMMAND "${dxc}" -T "${profile}" -E "${entry}" ${flags} -Fo "${output}" "${shader_source}"
        DEPENDS "${shader_source}" "${dxc}" ${shader_includes} VERBATIM)
      set_property(GLOBAL APPEND PROPERTY FLOP_SHADER_OUTPUTS "${output}")
      set_property(GLOBAL APPEND_STRING PROPERTY FLOP_SHADER_MANIFEST
        "${SHADER_ID}|${stage}|${format}|${entry}|${output}|${reflection}|${SHADER_RESOURCE_FREE}|${dxil_reflection}\n")
    endforeach()
    set(spirv_file "${shader_dir}/${SHADER_ID}.${stage}.spirv")
    add_custom_command(OUTPUT "${reflection}"
      COMMAND "${CMAKE_COMMAND}" "-DREFLECT=$<TARGET_FILE:flop_spirv_reflect>"
        "-DSHADER=${spirv_file}" "-DOUTPUT=${reflection}" "-DID=${SHADER_ID}"
        "-DSTAGE=${stage}" "-DENTRY=${entry}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflect_shader.cmake"
      DEPENDS "${spirv_file}" flop_spirv_reflect "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflect_shader.cmake" VERBATIM)
    set_property(GLOBAL APPEND PROPERTY FLOP_SHADER_OUTPUTS "${reflection}")
    if(TARGET flop_dxil_reflect)
      set(dxil_file "${shader_dir}/${SHADER_ID}.${stage}.dxil")
      add_custom_command(OUTPUT "${dxil_reflection}"
        COMMAND "${CMAKE_COMMAND}" "-DREFLECT=$<TARGET_FILE:flop_dxil_reflect>"
          "-DDXCOMPILER=${flop_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
          "-DSHADER=${dxil_file}" "-DOUTPUT=${dxil_reflection}" "-DID=${SHADER_ID}"
          "-DSTAGE=${stage}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflect_dxil.cmake"
        DEPENDS "${dxil_file}" flop_dxil_reflect "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflect_dxil.cmake" VERBATIM)
      set(comparison "${shader_dir}/${SHADER_ID}.${stage}.validated")
      add_custom_command(OUTPUT "${comparison}"
        COMMAND "${CMAKE_COMMAND}" "-DSPIRV=${reflection}" "-DDXIL=${dxil_reflection}"
          "-DCOMPARE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/compare_shader_reflection.cmake"
          "-DOUTPUT=${comparison}" "-DID=${SHADER_ID}" "-DSTAGE=${stage}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/validate_shader.cmake"
        DEPENDS "${reflection}" "${dxil_reflection}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/compare_shader_reflection.cmake"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/validate_shader.cmake" VERBATIM)
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
  add_custom_command(OUTPUT "${shader_header}"
    COMMAND "${CMAKE_COMMAND}" "-DOUTPUT=${shader_header}" "-DINPUT=${shader_manifest}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake"
    DEPENDS ${shader_outputs} "${shader_manifest}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake" VERBATIM)
  target_sources(${target} PRIVATE "${shader_header}")
  target_include_directories(${target} PRIVATE "${shader_dir}")
endfunction()
