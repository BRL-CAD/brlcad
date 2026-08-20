if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --reject-invalid-objs --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 3)
  message(FATAL_ERROR
    "AP214 non-manifold four-use fixture returned ${import_result}, "
    "expected 3\n${import_output}\n${import_error}")
endif()

# AP214 root #3622 declares a MANIFOLD_SOLID_BREP while reusing each of
# fifteen EDGE_CURVEs four times in one CLOSED_SHELL.  This reduced fixture
# retains representative edge #81451 and its four distinct face uses.  The
# importer must report the unprovable pairing and skip the invalid solid; it
# must never guess a pairing or emit approximate fallback geometry.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "\"entity_id\":81451"
    "did not have exactly two uses in every shell component"
    "closed STEP BREP did not validate as a solid")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
