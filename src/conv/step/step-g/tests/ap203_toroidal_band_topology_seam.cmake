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
    "toroidal topology-seam band import failed (${result})\n${stdout}\n${stderr}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "split an open STEP boundary edge at an exact OpenNURBS periodic seam"
    "inserted an exact OpenNURBS seam for an implicit periodic STEP face band")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep AP203_Toroidal_Band_Topology_Seam_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO, Plate mode: YES" OR
    NOT brep_text MATCHES "faces:[ ]+1" OR
    NOT brep_text MATCHES "loops:[ ]+1")
  message(FATAL_ERROR
    "toroidal topology-seam BREP validation failed\n${brep_text}")
endif()
