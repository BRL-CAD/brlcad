if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()
file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "bounded surface-curve import returned ${result}:\n${log}${error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"SURFACE_CURVE\":2"
    "\"PCURVE\":2"
    "\"BOUNDED_CURVE\":2"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "bounded surface-curve report omits ${expected}:\n${report_text}")
  endif()
endforeach()
if(report_text MATCHES "\"COMPLEX_ENTITY\"")
  message(FATAL_ERROR "complex component keywords were not expanded:\n${report_text}")
endif()
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "brep Bounded_Surface_Curves_wire.s info"
  OUTPUT_VARIABLE brep_output ERROR_VARIABLE brep_error)
set(brep_text "${brep_output}${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "edges:[ ]+2" OR
   NOT brep_text MATCHES "3d curve:[ ]+2")
  message(FATAL_ERROR "bounded surface-curve geometry validation failed:\n${brep_text}")
endif()
