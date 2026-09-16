if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
  TIMEOUT 60
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "bounded parent curve import failed (${result})\n${output}\n${error}")
endif()

file(READ "${REPORT}" report)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"repairs\":0")
  string(FIND "${report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report}")
  endif()
endforeach()
