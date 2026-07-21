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
    "inserted an exact OpenNURBS seam for an implicit periodic STEP face band")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
foreach(unexpected
    "aligned a closed surface seam with an exact pole cut"
    "inserted an exact paired seam cut to a surface pole")
  string(FIND "${report_text}" "${unexpected}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "inner face bound was incorrectly treated as a spherical cap (${unexpected}):\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Spherical_Band_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1")
  message(FATAL_ERROR "spherical band BREP validation failed\n${brep_text}")
endif()
