if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "AP214 measured full-period fixture failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

# AP214 root #3705 used two spherical boundary arcs whose final join was one
# surface period apart and whose measured 3-D residual was 0.00899 mm under a
# declared 0.01 mm tolerance.  The repair must remain tied to the shared STEP
# vertex and must leave a structurally valid exact surface model.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep measured_full_period_chain_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO")
  message(FATAL_ERROR
    "AP214 measured full-period BREP validation failed:\n${brep_text}")
endif()
