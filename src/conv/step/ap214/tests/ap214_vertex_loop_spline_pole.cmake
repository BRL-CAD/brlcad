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
  message(FATAL_ERROR
    "vertex-loop spline pole import returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"inferred_curves\":0"
    "represented the exact periodic pole vertex loop as the singular side")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Vertex_Loop_Spline_Pole_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+2" OR
   NOT brep_text MATCHES "vertices:[ ]+2" OR
   NOT brep_text MATCHES "loops:[ ]+1" OR
   NOT brep_text MATCHES "trims:[ ]+4")
  message(FATAL_ERROR "vertex-loop spline pole BREP validation failed\n${brep_text}")
endif()

set(exact_report "${REPORT}.exact")
set(exact_output "${OUTPUT}.exact")
file(REMOVE "${exact_report}" "${exact_output}")
execute_process(
  COMMAND "${STEP_G}" --exact --strict --reject-invalid-objs
    -O "${exact_output}" --report "${exact_report}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_stdout
  ERROR_VARIABLE exact_stderr
)
if(NOT exact_result EQUAL 0)
  message(FATAL_ERROR
    "exact vertex-loop spline pole import returned ${exact_result}\n${exact_stdout}\n${exact_stderr}")
endif()
file(READ "${exact_report}" exact_report_text)
foreach(expected
    "\"geometry_written\":1"
    "\"invalid_breps\":0"
    "\"inferred_curves\":0")
  string(FIND "${exact_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "exact report does not contain ${expected}:\n${exact_report_text}")
  endif()
endforeach()
