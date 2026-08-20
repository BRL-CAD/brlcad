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
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "step-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"GEOMETRICALLY_BOUNDED_WIREFRAME_SHAPE_REPRESENTATION\":1"
    "\"GEOMETRIC_CURVE_SET\":1"
    "\"BEZIER_CURVE\":2"
    "\"RATIONAL_B_SPLINE_CURVE\":1"
    "\"OFFSET_CURVE_3D\":1"
    "\"products\":1"
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

if(NOT report_text MATCHES "constructed an adaptive offset polyline within the model tolerance")
  message(FATAL_ERROR "report does not contain the adaptive offset diagnostic:\n${report_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Derived_Curve_Product_item.s info C3 1
  OUTPUT_VARIABLE rational_output
  ERROR_VARIABLE rational_error
)
set(rational_text "${rational_output}\n${rational_error}")
if(NOT rational_text MATCHES "ON_NurbsCurve dim = 3 is_rat = 1" OR
   NOT rational_text MATCHES "CV\\[[ ]*1\\] \\[5, 15, 4, 0.5\\] = \\(10, 30, 8\\)")
  message(FATAL_ERROR "rational Bezier validation failed\n${rational_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Derived_Curve_Product_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+0" OR
   NOT brep_text MATCHES "edges:[ ]+3" OR
   NOT brep_text MATCHES "3d curve:[ ]+3" OR
   NOT brep_text MATCHES "vertices:[ ]+6")
  message(FATAL_ERROR "Bezier/offset wire BREP validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" get Derived_Curve_Product_item
  OUTPUT_VARIABLE comb_output
  ERROR_VARIABLE comb_error
)
set(comb_text "${comb_output}\n${comb_error}")
if(NOT comb_text MATCHES "comb region no")
  message(FATAL_ERROR "zero-thickness wireframe was incorrectly promoted to a region\n${comb_text}")
endif()
