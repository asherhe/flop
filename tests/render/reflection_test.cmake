foreach(fixture IN ITEMS reflection reflection_invalid reflection_separate)
  execute_process(COMMAND "${DXC}" -T ps_6_0 -E fragment -spirv -fspv-target-env=vulkan1.0
    -Fo "${OUTPUT_DIR}/${fixture}.spirv" "${FIXTURE_DIR}/${fixture}.hlsl"
    RESULT_VARIABLE result ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Fixture compilation failed: ${error}")
  endif()
  execute_process(COMMAND "${REFLECT}" "${OUTPUT_DIR}/${fixture}.spirv"
    RESULT_VARIABLE result OUTPUT_VARIABLE json ERROR_VARIABLE error)
  if(fixture STREQUAL reflection)
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "Reflection failed: ${error}")
    endif()
    string(JSON stage GET "${json}" stage)
    string(JSON member GET "${json}" uniform_buffers 0 members 0 name)
    string(JSON offset GET "${json}" uniform_buffers 0 members 0 offset)
    string(JSON texture GET "${json}" textures 0 name)
    if(NOT stage STREQUAL "fragment" OR NOT member STREQUAL "tint" OR
       NOT offset EQUAL 0 OR NOT texture STREQUAL "albedo")
      message(FATAL_ERROR "Unexpected reflection: ${json}")
    endif()
  elseif(fixture STREQUAL reflection_invalid AND (result EQUAL 0 OR NOT error MATCHES "consecutive"))
    message(FATAL_ERROR "Invalid binding accepted or wrong diagnostic: ${error}")
  elseif(fixture STREQUAL reflection_separate AND (result EQUAL 0 OR NOT error MATCHES "separate textures"))
    message(FATAL_ERROR "Separate texture accepted or wrong diagnostic: ${error}")
  endif()
endforeach()

# the Vulkan attribute must also compile for the Direct3D variant.
execute_process(COMMAND "${DXC}" -T ps_6_0 -E fragment
  -Fo "${OUTPUT_DIR}/reflection.dxil" "${FIXTURE_DIR}/reflection.hlsl"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DXIL fixture compilation failed: ${error}")
endif()
