if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "ap214-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"COMPOSITE_CURVE\":1"
    "\"COMPOSITE_CURVE_SEGMENT\":3"
    "\"POLYLINE\":3"
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
  COMMAND "${MGED}" -c "${OUTPUT}" brep Composite_Product_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+0" OR
   NOT brep_text MATCHES "edges:[ ]+1" OR
   NOT brep_text MATCHES "3d curve:[ ]+1" OR
   NOT brep_text MATCHES "vertices:[ ]+2")
  message(FATAL_ERROR "composite wire BREP validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Composite_Product_item.s info C3 0
  OUTPUT_VARIABLE curve_output
  ERROR_VARIABLE curve_error
)
set(curve_text "${curve_output}\n${curve_error}")
if(NOT curve_text MATCHES "order = 2 cv_count = 4" OR
   NOT curve_text MATCHES "CV\[[ ]*0\] \(0, 0, 0\)" OR
   NOT curve_text MATCHES "CV\[[ ]*1\] \(10, 0, 0\)" OR
   NOT curve_text MATCHES "CV\[[ ]*2\] \(10, 10, 0\)" OR
   NOT curve_text MATCHES "CV\[[ ]*3\] \(20, 10, 0\)")
  message(FATAL_ERROR "composite curve geometry validation failed\n${curve_text}")
endif()
