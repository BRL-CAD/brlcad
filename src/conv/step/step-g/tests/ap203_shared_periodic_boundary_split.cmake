if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --report "${REPORT}" "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
  TIMEOUT 60
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "shared periodic boundary import failed (${result})\n${stdout}\n${stderr}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"message\":\"rolled back native-domain edge remaps which opened an exact cyclic join\",\"count\":1"
    "\"message\":\"resolved a contradictory analytic proxy tangent using directed exact-edge chords\",\"count\":1")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep Shared_Periodic_Boundary_Split_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO, Plate mode: YES" OR
    NOT brep_text MATCHES "faces:[ ]+4" OR
    # Native-domain remaps are transactional.  The remap which would open
    # this cyclic join is rolled back, preserving the twelve authoritative
    # source edges and their fifteen exact directed uses without introducing
    # a synthetic topology split.
    NOT brep_text MATCHES "edges:[ ]+12" OR
    NOT brep_text MATCHES "loops:[ ]+4" OR
    NOT brep_text MATCHES "trims:[ ]+15")
  message(FATAL_ERROR
    "shared periodic boundary BREP validation failed\n${brep_text}")
endif()
