if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT_SAFE OR
    NOT DEFINED OUTPUT_NONE OR NOT DEFINED REPORT_SAFE OR NOT DEFINED REPORT_NONE)
  message(FATAL_ERROR "AP214_G, INPUT, outputs, and reports are required")
endif()

file(REMOVE "${OUTPUT_SAFE}" "${OUTPUT_NONE}" "${REPORT_SAFE}" "${REPORT_NONE}")
execute_process(
  COMMAND "${AP214_G}" --repair safe -O "${OUTPUT_SAFE}" --report "${REPORT_SAFE}" "${INPUT}"
  RESULT_VARIABLE safe_result
  OUTPUT_VARIABLE safe_output
  ERROR_VARIABLE safe_error
)
execute_process(
  COMMAND "${AP214_G}" --repair none -O "${OUTPUT_NONE}" --report "${REPORT_NONE}" "${INPUT}"
  RESULT_VARIABLE none_result
  OUTPUT_VARIABLE none_output
  ERROR_VARIABLE none_error
)
if(NOT safe_result EQUAL 0 OR NOT none_result EQUAL 0)
  message(FATAL_ERROR
    "repair policy imports failed (${safe_result}, ${none_result})\n"
    "${safe_output}${safe_error}${none_output}${none_error}")
endif()

file(READ "${REPORT_SAFE}" safe_report)
file(READ "${REPORT_NONE}" none_report)
foreach(report IN ITEMS safe_report none_report)
  if(NOT "${${report}}" MATCHES "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0")
    message(FATAL_ERROR "${report} does not show one successful exact geometry item")
  endif()
endforeach()
if(NOT safe_report MATCHES "\"repair\":\"safe\"" OR
   NOT safe_report MATCHES "\"repairs\":2" OR
   NOT safe_report MATCHES "snapped swept profile endpoint within model tolerance\"[^}]*\"count\":2")
  message(FATAL_ERROR "safe report does not contain two bounded endpoint repairs:\n${safe_report}")
endif()
if(NOT none_report MATCHES "\"repair\":\"none\"" OR
   NOT none_report MATCHES "\"repairs\":0" OR
   none_report MATCHES "snapped swept profile endpoint")
  message(FATAL_ERROR "repair-none report records a forbidden repair:\n${none_report}")
endif()

file(SHA256 "${OUTPUT_SAFE}" safe_hash)
file(SHA256 "${OUTPUT_NONE}" none_hash)
if(safe_hash STREQUAL none_hash)
  message(FATAL_ERROR "safe and none repair modes unexpectedly produced identical databases")
endif()
