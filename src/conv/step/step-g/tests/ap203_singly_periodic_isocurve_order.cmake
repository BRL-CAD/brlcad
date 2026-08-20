if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --entity 100176 --report "${REPORT}" "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
  TIMEOUT 90
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "singly-periodic repair-order import failed (${result})\n${stdout}\n${stderr}")
endif()

# This is the dependency closure of one 23-face AP203 solid.  Its singly
# periodic cylindrical face contains a zero-area/keyhole subchain around a
# same-face seam pair.  Trying a branch-agnostic seam split before the exact,
# uniquely bounded isocurve coordinate leaves that seam in an inner loop.
# The repair must preserve the complete shell and reach the subsequent
# closed-edge orientation proof.  That proof must be applied as one compatible
# parity transaction; validating the complete BREP once per candidate loop
# made large patterned versions of this topology effectively quadratic.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "corrected a complete split closed-edge loop from closed-shell edge-use constraints"
    "resolved compatible split-edge loop orientations in one parity-graph transaction")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep step_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: YES, Plate mode: NO" OR
    NOT brep_text MATCHES "faces:[ ]+23")
  message(FATAL_ERROR
    "singly-periodic repair-order BREP validation failed:\n${brep_text}")
endif()
