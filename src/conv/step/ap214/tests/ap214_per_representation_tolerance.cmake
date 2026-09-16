if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED OVERRIDE_REPORT)
  message(FATAL_ERROR
    "STEP_G, INPUT, REPORT, and OVERRIDE_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${OVERRIDE_REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --repair safe --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "per-representation tolerance import failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

# Representation #86 declares 1e-5 mm uncertainty and contains two 5e-6 mm
# endpoint residuals.  Unreferenced context #88 declares 1e-6 mm, but must not
# constrain #86.  This is the compact guard for AP214 solid #3664, which
# was rejected when the importer incorrectly used the file-wide minimum.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "snapped swept profile endpoint within model tolerance")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

# An explicit command-line tolerance remains authoritative and should reject
# the same residual when tightened to 1e-6 mm.
execute_process(
  COMMAND "${STEP_G}" -D --repair safe --abs-tol 0.000001
    --report "${OVERRIDE_REPORT}" "${INPUT}"
  RESULT_VARIABLE override_result
  OUTPUT_VARIABLE override_output
  ERROR_VARIABLE override_error
)
if(override_result EQUAL 0)
  message(FATAL_ERROR
    "--abs-tol unexpectedly accepted a profile gap above the override\n"
    "${override_output}\n${override_error}")
endif()
file(READ "${OVERRIDE_REPORT}" override_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "profile boundary gap exceeds the model tolerance")
  string(FIND "${override_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "override report does not contain ${expected}:\n${override_text}")
  endif()
endforeach()
