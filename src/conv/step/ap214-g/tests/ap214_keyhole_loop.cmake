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
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"repairs\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Keyhole_Surface_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+9" OR
   NOT brep_text MATCHES "loops:[ ]+1")
  message(FATAL_ERROR "keyhole surface BREP validation failed\n${brep_text}")
endif()

set(none_report "${REPORT}.none")
file(REMOVE "${none_report}")
execute_process(
  COMMAND "${AP214_G}" -D --repair none --report "${none_report}" "${INPUT}"
  RESULT_VARIABLE none_result
  OUTPUT_VARIABLE none_output
  ERROR_VARIABLE none_error
)
if(NOT none_result EQUAL 0)
  message(FATAL_ERROR "unrepaired keyhole returned ${none_result}, expected 0\n${none_output}\n${none_error}")
endif()
file(READ "${none_report}" none_text)
if(NOT none_text MATCHES "\"geometry_written\":1" OR
   NOT none_text MATCHES "\"geometry_skipped\":0" OR
   NOT none_text MATCHES "\"invalid_breps\":0" OR
   NOT none_text MATCHES "\"repairs\":0")
  message(FATAL_ERROR "--repair none did not preserve the valid keyhole seam:\n${none_text}")
endif()
