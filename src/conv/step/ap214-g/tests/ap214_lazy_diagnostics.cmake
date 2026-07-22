if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "AP214_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${AP214_G}" -D --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 3)
  message(FATAL_ERROR "malformed diagnostic fixture returned ${import_result}, expected 3\n${import_output}\n${import_error}")
endif()
if(NOT import_error MATCHES
    "reference to missing instance #99 \\[representative=#1\\] \\(count=2\\)")
  message(FATAL_ERROR
    "bounded text diagnostic lacks a representative entity or exact count\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
if(NOT report_text MATCHES
    "\"entity_id\":1,\"entity_type\":\"REPRESENTATION_RELATIONSHIP\",\"file_offset\":[1-9][0-9]*,\"line\":[0-9]+,\"attribute\":\"\",\"message\":\"reference to missing instance #99\",\"count\":2,\"aggregated\":true")
  message(FATAL_ERROR
    "default structured report lacks a representative source context or exact count:\n${report_text}")
endif()

set(verbose_report "${REPORT}.verbose")
file(REMOVE "${verbose_report}")
execute_process(
  COMMAND "${AP214_G}" -D -v --report "${verbose_report}" "${INPUT}"
  RESULT_VARIABLE verbose_result
  OUTPUT_VARIABLE verbose_output
  ERROR_VARIABLE verbose_error
)
if(NOT verbose_result EQUAL 3)
  message(FATAL_ERROR
    "verbose malformed diagnostic fixture returned ${verbose_result}, expected 3\n${verbose_output}\n${verbose_error}")
endif()
file(READ "${verbose_report}" verbose_report_text)
if(NOT verbose_report_text MATCHES
    "\"entity_id\":1,\"entity_type\":\"REPRESENTATION_RELATIONSHIP\",\"file_offset\":[1-9][0-9]*,\"line\":[0-9]+,\"attribute\":\"\",\"message\":\"reference to missing instance #99\",\"count\":2,\"aggregated\":false")
  message(FATAL_ERROR
    "verbose structured diagnostic report lacks source context or exact count:\n${verbose_report_text}")
endif()
