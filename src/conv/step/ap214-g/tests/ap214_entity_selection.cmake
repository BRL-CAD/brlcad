if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "AP214_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${AP214_G}" -D --entity "#200,207" -e 263 --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE selected_result
  OUTPUT_VARIABLE selected_output
  ERROR_VARIABLE selected_error
)
if(NOT selected_result EQUAL 0)
  message(FATAL_ERROR
    "selected AP214 import failed (${selected_result}):\n${selected_output}\n${selected_error}")
endif()

file(READ "${REPORT}" report_text)
if(NOT report_text MATCHES "\"selected_entity_ids\":\[200,207,263\]")
  message(FATAL_ERROR "report did not retain the normalized selection:\n${report_text}")
endif()
if(NOT report_text MATCHES
    "\"selected_entities_matched\":3,\"geometry_attempted\":3,\"geometry_written\":3,\"geometry_skipped\":0")
  message(FATAL_ERROR "entity selection did not isolate the three requested roots:\n${report_text}")
endif()

execute_process(
  COMMAND "${AP214_G}" -D --entity 0 "${INPUT}"
  RESULT_VARIABLE invalid_result
  OUTPUT_VARIABLE invalid_output
  ERROR_VARIABLE invalid_error
)
if(NOT invalid_result EQUAL 2)
  message(FATAL_ERROR
    "invalid entity selector returned ${invalid_result}, expected 2:\n${invalid_output}\n${invalid_error}")
endif()
