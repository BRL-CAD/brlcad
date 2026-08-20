if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED EXACT_REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, REPORT, and EXACT_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE safe_result
  OUTPUT_VARIABLE safe_output
  ERROR_VARIABLE safe_error
)
if(NOT safe_result EQUAL 0)
  message(FATAL_ERROR
    "safe near-native seam import failed (${safe_result}):\n"
    "${safe_output}${safe_error}")
endif()

file(READ "${REPORT}" safe_report)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "retained the native closed source-curve seam within its measured STEP vertex tolerance"
    "closed source curve missed its topology vertex; used a measured local edge tolerance while preserving the exact curve locus")
  string(FIND "${safe_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "safe report does not contain ${expected}:\n${safe_report}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR
    "exact near-native seam import returned ${exact_result}, expected 3:\n"
    "${exact_output}${exact_error}")
endif()

file(READ "${EXACT_REPORT}" exact_report)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1"
    "\"reason\":\"exact OpenNURBS conversion failed\""
    "could not relocate the closed source-curve seam")
  string(FIND "${exact_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "exact report does not contain ${expected}:\n${exact_report}")
  endif()
endforeach()
