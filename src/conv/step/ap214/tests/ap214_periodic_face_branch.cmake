if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

# This fixture regressed when an exact three-circle
# spherical boundary was remapped onto inconsistent periodic images.  The
# resulting join differed by precisely one U period and failed structural
# validation.  Keep the source face as an open shell so this guard is quick,
# while preserving the original edge, surface, and one-period branch geometry.
file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --exact -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 30
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "periodic face-branch fixture returned ${import_result}\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"opennurbs_structural_validation\":{\"calls\":1"
    "translated a regenerated pcurve onto its required periodic branch")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

string(FIND "${report_text}" "OpenNURBS structural validation failed"
  structural_failure)
if(NOT structural_failure EQUAL -1)
  message(FATAL_ERROR
    "periodic face branch regressed structurally:\n${report_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep Periodic_Face_Branch_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
foreach(expected
    "Valid: YES, Solid: NO"
    "faces:     1"
    "edges:     3"
    "vertices:  3"
    "loops:     1"
    "trims:     3")
  string(FIND "${brep_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "periodic face-branch BREP does not contain ${expected}:\n${brep_text}")
  endif()
endforeach()
