if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED OVERRIDE_REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR
    "STEP_G, INPUT, REPORT, OVERRIDE_REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OVERRIDE_REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "per-representation periodic-cap import failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

# The owning representation declares 0.01 mm uncertainty.  A deliberately
# unrelated context declares 0.000001 mm, which remains the conservative
# session tolerance reported below.  The two-face spherical/cylindrical shell
# succeeds only when the periodic-cap transaction uses the owning
# representation's uncertainty for dense edge/surface/vertex validation
# without using it for topology discovery.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"tolerance_mm\":1e-06"
    "aligned a closed surface seam with an exact pole cut"
    "inserted an exact paired seam cut to a surface pole")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

# Exact mode makes a command-line tolerance authoritative.  Tightening it to
# the unrelated context's value must reproduce the source endpoint
# contradiction instead of applying the default, explicitly reported
# source-reality tolerance adjustment.  Default permissive output retains and
# tags the unresolved BREP; strict policy is what requests omission.
execute_process(
  COMMAND "${STEP_G}" -D --exact --abs-tol 0.000001
    --report "${OVERRIDE_REPORT}" "${INPUT}"
  RESULT_VARIABLE override_result
  OUTPUT_VARIABLE override_output
  ERROR_VARIABLE override_error
  TIMEOUT 60
)
if(NOT override_result EQUAL 1)
  message(FATAL_ERROR
    "tight tolerance import returned ${override_result}, expected 1\n"
    "${override_output}\n${override_error}")
endif()
file(READ "${OVERRIDE_REPORT}" override_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":1"
    "\"invalid_breps_written\":1"
    "exact STEP BREP construction failed after producing a storage-safe partial BREP"
    "exceeds the exact model tolerance 1e-06")
  string(FIND "${override_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "override report does not contain ${expected}:\n${override_text}")
  endif()
endforeach()
