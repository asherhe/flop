execute_process(COMMAND "${DXC}" -T ps_6_0 -E fragment
  -Fo "${OUTPUT_DIR}/dxil-reflection-fixture.dxil" "${FIXTURE_DIR}/reflection.hlsl"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DXIL compilation failed: ${error}")
endif()
execute_process(COMMAND "${REFLECT}" "${DXCOMPILER}" "${OUTPUT_DIR}/dxil-reflection-fixture.dxil"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DXIL reflection failed: ${error}")
endif()
foreach(expected IN ITEMS
    "resource Material type=0 space=3 binding=0 count=1"
    "resource albedo_sampler type=3 space=2 binding=0 count=1"
    "resource albedo type=2 space=2 binding=0 count=1"
    "buffer Material size=16"
    "member tint offset=0 size=16 type=3 rows=1 columns=4")
  string(FIND "${output}" "${expected}" index)
  if(index EQUAL -1)
    message(FATAL_ERROR "Missing '${expected}' in DXIL reflection output: ${output}")
  endif()
endforeach()
execute_process(COMMAND "${REFLECT}" "${DXCOMPILER}" "${FIXTURE_DIR}/reflection.hlsl"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "cannot reflect DXIL")
  message(FATAL_ERROR "Invalid DXIL was not rejected: ${error}")
endif()
