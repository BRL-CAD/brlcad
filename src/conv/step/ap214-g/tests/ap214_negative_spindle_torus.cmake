if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -v -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "safe import returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"entity_id\":56,\"entity_type\":\"TOROIDAL_SURFACE\""
    "using the exact, schema-valid radius magnitude 10"
    "normalized a negative torus radius by exact locus-preserving reparameterization")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "safe report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Negative_Spindle_Torus_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+4" OR
   NOT brep_text MATCHES "trims:[ ]+4")
  message(FATAL_ERROR "negative spindle-torus BREP validation failed\n${brep_text}")
endif()

set(exact_report "${REPORT}.exact")
file(REMOVE "${exact_report}")
execute_process(
  COMMAND "${AP214_G}" -D --exact --report "${exact_report}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR "--exact returned ${exact_result}, expected 3\n${exact_output}\n${exact_error}")
endif()
file(READ "${exact_report}" exact_text)
foreach(expected
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "\"repairs\":0"
    "major_radius=-10 violates the AP positive_length_measure constraint")
  string(FIND "${exact_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "exact report does not contain ${expected}:\n${exact_text}")
  endif()
endforeach()
