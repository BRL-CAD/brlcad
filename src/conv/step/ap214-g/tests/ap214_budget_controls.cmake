if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "AP214_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${AP214_G}" -D -j 2 --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
  TIMEOUT 30
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "automatic budget calibration failed (${result}):\n${output}\n${error}")
endif()
if(NOT EXISTS "${REPORT}")
  message(FATAL_ERROR "automatic budget calibration did not write its report")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"requested_budget_scale\":0"
    "\"item_cpu_budgets_enabled\":true"
    "\"item_budget_clock\":\"critical_path_cpu\""
    "\"calibration\":{\"ran\":true,\"valid\":true")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "automatic budget report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
if(NOT report_text MATCHES "\"effective_budget_scale\":[0-9]+(\\.[0-9]+)?" OR
   NOT report_text MATCHES "\"effective_item_cpu_budget_ms\":[1-9][0-9]*" OR
   NOT report_text MATCHES "\"effective_stall_timeout_ms\":[1-9][0-9]*" OR
   NOT report_text MATCHES "\"scalar_queries_per_second\":[1-9][0-9]*" OR
   NOT report_text MATCHES "\"parallel_cpu_queries_per_second\":[1-9][0-9]*")
  message(FATAL_ERROR "automatic budget report lacks positive calibrated values:\n${report_text}")
endif()
