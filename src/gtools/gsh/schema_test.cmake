if(NOT DEFINED GSH_EXECUTABLE)
  message(FATAL_ERROR "GSH_EXECUTABLE is not defined")
endif()

execute_process(
  COMMAND "${GSH_EXECUTABLE}" --command-schema draw
  RESULT_VARIABLE single_result
  OUTPUT_VARIABLE single_json
  ERROR_VARIABLE single_error
  TIMEOUT 30
)
if(NOT single_result EQUAL 0)
  message(FATAL_ERROR "single schema export failed: ${single_error}")
endif()
string(JSON single_kind ERROR_VARIABLE single_json_error GET "${single_json}" kind)
if(single_json_error OR NOT single_kind STREQUAL "native")
  message(FATAL_ERROR "invalid draw schema JSON (${single_json_error}): ${single_json}")
endif()

execute_process(
  COMMAND "${GSH_EXECUTABLE}" --command-schema all
  RESULT_VARIABLE all_result
  OUTPUT_VARIABLE all_json
  ERROR_VARIABLE all_error
  TIMEOUT 30
)
if(NOT all_result EQUAL 0)
  message(FATAL_ERROR "registry schema export failed: ${all_error}")
endif()
string(JSON schema_count ERROR_VARIABLE all_json_error LENGTH "${all_json}")
if(all_json_error OR schema_count LESS 100)
  message(FATAL_ERROR "invalid registry schema JSON (${all_json_error}, count=${schema_count})")
endif()
