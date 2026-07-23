if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR
   NOT DEFINED REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, OUTPUT, and REPORT are required")
endif()

file(REMOVE "${OUTPUT}" "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -e 91 --budget-scale 2 --item-budget 7
          --no-item-budget --stall-timeout 3 --report "${REPORT}"
          "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
  TIMEOUT 60
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "step-g positional AP203 import failed (${result})\n${stdout}\n${stderr}")
endif()
if(NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR "step-g did not create its positional output")
endif()
if(NOT EXISTS "${REPORT}")
  message(FATAL_ERROR "step-g did not create its structured report")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"requested_budget_scale\":2"
    "\"effective_budget_scale\":2"
    "\"item_cpu_budgets_enabled\":false"
    "\"effective_item_cpu_budget_ms\":0"
    "\"item_budget_clock\":\"critical_path_cpu\""
    "\"effective_stall_timeout_ms\":3000"
    "\"calibration\":{\"ran\":false")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "step-g budget report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
string(FIND "${stdout}${stderr}" "STEP lazy cache: indexed=" lazy_summary)
if(lazy_summary EQUAL -1)
  message(FATAL_ERROR "step-g did not report use of the lazy index\n${stdout}\n${stderr}")
endif()
string(FIND "${stdout}${stderr}" "current-loaded=0, current-pinned=0" released_summary)
if(released_summary EQUAL -1)
  message(FATAL_ERROR "the final AP203 dependency batch was not fully released\n${stdout}\n${stderr}")
endif()
