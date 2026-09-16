if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED EXPECTED_REPAIR)
  message(FATAL_ERROR
    "STEP_G, INPUT, REPORT, and EXPECTED_REPAIR are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --reject-invalid-objs --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)

# Each minimized closure deliberately retains only the faces needed to
# exercise its repair.  The resulting CLOSED_SHELL cannot be a solid, but its
# BREP must be structurally valid after the bounded topology transaction.
if(NOT import_result EQUAL 3)
  message(FATAL_ERROR
    "minimized structural fixture returned ${import_result}, expected 3\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "\"reason\":\"closed STEP BREP did not validate as a solid\""
    "${EXPECTED_REPAIR}")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

string(FIND "${report_text}" "OpenNURBS structural validation failed"
  structural_failure)
if(NOT structural_failure EQUAL -1)
  message(FATAL_ERROR
    "bounded repair remained structurally invalid:\n${report_text}")
endif()
