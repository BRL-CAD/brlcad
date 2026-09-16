if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "step-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"BOUNDED_CURVE\":1"
    "\"SURFACE_CURVE\":1"
    "\"NAMED_UNIT\":3"
    "\"GEOMETRIC_REPRESENTATION_CONTEXT\":2"
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "inserted an exact pole cut for a multi-edge full-period boundary")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
if(report_text MATCHES "\"COMPLEX_ENTITY\"")
  message(FATAL_ERROR "complex component keywords were not expanded:\n${report_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Multi_Edge_Conical_Cap_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+2" OR
   NOT brep_text MATCHES "edges:[ ]+11" OR
   NOT brep_text MATCHES "trims:[ ]+15")
  message(FATAL_ERROR "multi-edge conical cap BREP validation failed\n${brep_text}")
endif()
