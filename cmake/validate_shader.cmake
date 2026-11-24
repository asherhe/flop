execute_process(COMMAND "${CMAKE_COMMAND}" "-DSPIRV=${SPIRV}" "-DDXIL=${DXIL}" -P "${COMPARE}"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Shader '${ID}', stage '${STAGE}': SPIR-V/DXIL validation failed: ${error}")
endif()
file(WRITE "${OUTPUT}" "validated\n")
