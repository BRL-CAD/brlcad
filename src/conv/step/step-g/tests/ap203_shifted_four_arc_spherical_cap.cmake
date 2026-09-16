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
    "shifted four-arc spherical cap failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "aligned a closed surface seam with an exact pole cut"
    "resolved a contradictory analytic proxy tangent using directed exact-edge chords")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep AP203_Shifted_Four_Arc_Spherical_Cap_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
    NOT brep_text MATCHES "faces:[ ]+2" OR
    # The reciprocal STEP faces are complementary spherical caps.  Each cap
    # needs its own exact meridian cut to the opposite OpenNURBS pole; sharing
    # one cut would make both faces select the same physical cap.
    NOT brep_text MATCHES "edges:[ ]+6" OR
    NOT brep_text MATCHES "vertices:[ ]+6" OR
    NOT brep_text MATCHES "trims:[ ]+14")
  message(FATAL_ERROR
    "shifted four-arc spherical BREP validation failed\n${brep_text}")
endif()
