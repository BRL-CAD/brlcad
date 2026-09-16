if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED EXACT_REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR
    "STEP_G, MGED, INPUT, REPORT, EXACT_REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "AP214 measured periodic-winding fixture failed "
    "(${import_result})\n${import_output}\n${import_error}")
endif()

# AP214 root #3621 contains cylindrical face #149641.  Its two-edge
# boundary is topologically closed and follows one complete surface period,
# but the independently fitted B-spline edge and cylinder disagree by about
# 2.6e-4 mm despite a declared 1e-6 mm uncertainty.  Safe mode may use the
# previously densely measured local OpenNURBS tolerance to recognize the
# winding and then independently split the immutable 3-D edge at the native
# seam.  It must not relax this proof in --exact mode.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "accepted a topology-proven one-period boundary using measured source tolerance"
    "source periodic boundary winding exceeded the declared tolerance")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep measured_periodic_winding_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO")
  message(FATAL_ERROR
    "AP214 measured periodic-winding BREP validation failed:\n"
    "${brep_text}")
endif()

execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
  TIMEOUT 60
)
if(NOT exact_result EQUAL 1)
  message(FATAL_ERROR
    "AP214 measured periodic-winding exact import returned "
    "${exact_result}, expected 1\n${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":1"
    "\"invalid_breps_written\":1"
    "exact STEP BREP construction failed after producing a storage-safe partial BREP")
  string(FIND "${exact_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "exact report does not contain ${expected}:\n${exact_text}")
  endif()
endforeach()
foreach(forbidden
    "accepted a topology-proven one-period boundary using measured source tolerance"
    "source periodic boundary winding exceeded the declared tolerance")
  string(FIND "${exact_text}" "${forbidden}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR
      "exact import applied the safe measured-winding repair:\n${exact_text}")
  endif()
endforeach()
