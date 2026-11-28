add_executable(flop_spirv_reflect spirv_reflect.cpp)
target_link_libraries(flop_spirv_reflect PRIVATE spirv-cross-core)
set_target_properties(flop_spirv_reflect PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

if(CMAKE_HOST_WIN32 AND WIN32)
  add_executable(flop_dxil_reflect dxil_reflect.cpp)
  target_include_directories(flop_dxil_reflect SYSTEM PRIVATE "${flop_dxc_SOURCE_DIR}/inc")
  set_target_properties(flop_dxil_reflect PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()
