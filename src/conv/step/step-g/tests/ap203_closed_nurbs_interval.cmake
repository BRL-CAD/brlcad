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
  TIMEOUT 60
)
if(NOT safe_result EQUAL 0)
  message(FATAL_ERROR
    "closed NURBS interval safe import failed (${safe_result})\n${safe_output}\n${safe_error}")
endif()

file(READ "${REPORT}" safe_report)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "translated an exact pcurve onto the native periodic surface domain"
    "relocated a closed rational surface seam away from an exact STEP boundary")
  string(FIND "${safe_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "safe report does not contain ${expected}:\n${safe_report}")
  endif()
endforeach()

# The surface radius and authoritative 3-D edges intentionally differ by
# 0.001 mm while the fixture declares 0.00001 mm.  Safe mode preserves both
# supplied loci and records their measured relationship; --exact must reject
# that same source geometry rather than inheriting the adjusted tolerance.
execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
  TIMEOUT 60
)
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR
    "closed NURBS interval exact import returned ${exact_result}, expected 3\n${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_report)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "\"repairs\":0"
    "source curve/surface separation 0.001 exceeds the exact model tolerance")
  string(FIND "${exact_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "exact report does not contain ${expected}:\n${exact_report}")
  endif()
endforeach()
