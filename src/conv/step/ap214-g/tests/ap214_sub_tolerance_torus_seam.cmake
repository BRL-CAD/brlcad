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
    "\"invalid_breps\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
if(report_text MATCHES "\"severity\":\"warning\"" OR
   report_text MATCHES "adjusted affected OpenNURBS edge tolerances")
  message(FATAL_ERROR "exact sub-tolerance torus patch was misclassified as source mismatch:\n${report_text}")
endif()

set(exact_report "${REPORT}.exact")
file(REMOVE "${exact_report}")
execute_process(
  COMMAND "${AP214_G}" -D --exact --report "${exact_report}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(NOT exact_result EQUAL 0)
  message(FATAL_ERROR "--exact rejected the exact sub-tolerance torus patch (${exact_result})\n${exact_output}\n${exact_error}")
endif()
file(READ "${exact_report}" exact_text)
if(NOT exact_text MATCHES "\"geometry_written\":1" OR
   exact_text MATCHES "\"severity\":\"warning\"")
  message(FATAL_ERROR "--exact did not preserve the sub-tolerance torus patch without tolerance adjustment:\n${exact_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Sub_Tolerance_Torus_Seam_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1")
  message(FATAL_ERROR "sub-tolerance torus seam BREP validation failed\n${brep_text}")
endif()
