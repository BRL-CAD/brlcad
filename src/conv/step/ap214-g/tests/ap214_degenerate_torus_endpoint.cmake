if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -j 1 --budget-scale 1 -O "${OUTPUT}"
    --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "ap214-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"message\":\"rolled back periodic seam resolution after exact edge-locus or topology endpoint validation failed\",\"count\":1")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep Degenerate_Torus_Endpoint_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
# Canonicalizing the horn-torus profile seam at its exact axis-collapse point
# retains the four source boundary edges.  OpenNURBS represents the singular
# endpoint and periodic seam with six trims sharing three topology vertices.
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO, Plate mode: YES" OR
    NOT brep_text MATCHES "faces:[ ]+1" OR
    NOT brep_text MATCHES "edges:[ ]+4" OR
    NOT brep_text MATCHES "vertices:[ ]+3" OR
    NOT brep_text MATCHES "loops:[ ]+1" OR
    NOT brep_text MATCHES "trims:[ ]+6")
  message(FATAL_ERROR "degenerate torus endpoint validation failed\n${brep_text}")
endif()
