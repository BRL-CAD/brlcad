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
    "\"GEOMETRICALLY_BOUNDED_WIREFRAME_SHAPE_REPRESENTATION\":1"
    "\"GEOMETRIC_SET\":1"
    "\"B_SPLINE_CURVE_WITH_KNOTS\":1"
    "\"CURVE_REPLICA\":1"
    "\"products\":1"
    "\"styles_applied\":1"
    "\"layers_extracted\":1"
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
  COMMAND "${MGED}" -c "${OUTPUT}" brep Wire_Product_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+0" OR
   NOT brep_text MATCHES "edges:[ ]+2" OR
   NOT brep_text MATCHES "3d curve:[ ]+2" OR
   NOT brep_text MATCHES "vertices:[ ]+4")
  message(FATAL_ERROR "wire BREP validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Wire_Product_item.s info C3 1
  OUTPUT_VARIABLE replica_output
  ERROR_VARIABLE replica_error
)
set(replica_text "${replica_output}\n${replica_error}")
if(NOT replica_text MATCHES "CV\\[[ ]*0\\] \\(30, 0, 0\\)" OR
   NOT replica_text MATCHES "CV\\[[ ]*3\\] \\(22, 40, 20\\)")
  message(FATAL_ERROR "curve-replica affine transform validation failed\n${replica_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" get Wire_Product_item
  OUTPUT_VARIABLE comb_output
  ERROR_VARIABLE comb_error
)
set(comb_text "${comb_output}\n${comb_error}")
if(NOT comb_text MATCHES "comb region no")
  message(FATAL_ERROR "zero-thickness wireframe was incorrectly promoted to a region\n${comb_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Wire_Product_item
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+11" OR
   NOT attr_text MATCHES "step:color_rgb[ ]+0.1 0.8 0.9" OR
   NOT attr_text MATCHES "step:layers[ ]+wire layer")
  message(FATAL_ERROR "wireframe metadata validation failed\n${attr_text}")
endif()
