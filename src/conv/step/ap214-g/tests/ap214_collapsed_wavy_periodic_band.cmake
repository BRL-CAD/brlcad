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
    "split a closed STEP boundary edge at an exact OpenNURBS periodic seam"
    "inserted an exact OpenNURBS seam for an implicit periodic STEP face band")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Collapsed_Wavy_Periodic_Band_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1")
  message(FATAL_ERROR "collapsed wavy periodic band BREP validation failed\n${brep_text}")
endif()

# The source spline exceeds its declared one-nanometre uncertainty.  Safe
# mode may use the densely measured local tolerance, but --exact must not let
# the native-seam candidate shift become a tolerance bypass.
set(exact_report "${REPORT}.exact")
file(REMOVE "${exact_report}")
execute_process(
  COMMAND "${AP214_G}" -D --exact --report "${exact_report}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR "ap214-g --exact returned ${exact_result}, expected 3\n${exact_output}\n${exact_error}")
endif()
