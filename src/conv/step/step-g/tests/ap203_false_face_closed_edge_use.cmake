if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --no-item-budget -e 4423 -O "${OUTPUT}"
    --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 30
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "false-face closed-edge fixture failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
string(CONCAT keyhole_pcurve_message
  "mirrored an exact interior keyhole pcurve onto its reciprocal "
  "STEP edge use")
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"tolerance_mm\":0.01"
    "${keyhole_pcurve_message}"
    "corrected a demonstrably inconsistent STEP loop orientation from closed-shell edge-use constraints")
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
    NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
    NOT brep_text MATCHES "faces:[ ]+6" OR
    NOT brep_text MATCHES "edges:[ ]+8" OR
    NOT brep_text MATCHES "vertices:[ ]+5" OR
    NOT brep_text MATCHES "trims:[ ]+16")
  message(FATAL_ERROR
    "false-face closed-edge BREP validation failed\n${brep_text}")
endif()
