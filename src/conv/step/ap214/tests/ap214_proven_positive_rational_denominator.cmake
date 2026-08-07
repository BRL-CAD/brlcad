if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT OR NOT DEFINED EXACT_REPORT)
  message(FATAL_ERROR
    "STEP_G, MGED, INPUT, REPORT, OUTPUT, and EXACT_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}" "${EXACT_REPORT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "positive-denominator import returned ${import_result}:\n"
    "${import_output}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "source weights include a nonpositive value"
    "analytically proving its homogeneous denominator remains positive"
    "lower bound 0.375"
    "v-direction Bezier rows")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "positive-denominator report omits ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep Proven_Positive_Rational_Denominator_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+4")
  message(FATAL_ERROR
    "positive-denominator BREP is invalid:\n${brep_text}")
endif()

execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(NOT exact_result EQUAL 0)
  message(FATAL_ERROR
    "--exact positive-denominator import returned ${exact_result}, expected 0:\n"
    "${exact_output}${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "source weights include a nonpositive value"
    "analytically proving its homogeneous denominator remains positive"
    "lower bound 0.375")
  string(FIND "${exact_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "exact positive-denominator report omits ${expected}:\n${exact_text}")
  endif()
endforeach()
