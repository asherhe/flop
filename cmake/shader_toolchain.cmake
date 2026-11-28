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
# Only package formats with the reflection support required to validate them.
if(CMAKE_HOST_WIN32 AND WIN32)
  set(FLOP_SHADER_FORMATS spirv dxil)
  set(dxcompiler "${flop_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(FLOP_SHADER_FORMATS spirv)
else()
  message(FATAL_ERROR "No validated shader format policy for ${CMAKE_SYSTEM_NAME}")
endif()
message(STATUS "Packaged shader formats: ${FLOP_SHADER_FORMATS}")
