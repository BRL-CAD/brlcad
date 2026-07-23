if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT OR NOT DEFINED EXACT_REPORT OR
   NOT DEFINED REJECT_INPUT OR NOT DEFINED REJECT_REPORT)
  message(FATAL_ERROR "NURBS terminal extension test arguments are incomplete")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}" "${EXACT_REPORT}" "${REJECT_INPUT}"
  "${REJECT_REPORT}")
execute_process(
  COMMAND "${AP214_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "safe ap214-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "extended an open NURBS terminal span to its asserted STEP vertex")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "safe report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep NURBS_Terminal_Extension_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1")
  message(FATAL_ERROR "terminal-extension BREP validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${AP214_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(exact_result EQUAL 0)
  message(FATAL_ERROR "--exact unexpectedly extended the source NURBS domain\n${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_report_text)
string(FIND "${exact_report_text}" "do not define a usable source interval" exact_found)
if(exact_found EQUAL -1)
  message(FATAL_ERROR "--exact report did not retain the source interval defect:\n${exact_report_text}")
endif()

# Exact continuation is authority only when it reaches the asserted vertex at
# the declared uncertainty.  Move that vertex off the terminal polynomial and
# verify that safe mode refuses the repair.
file(READ "${INPUT}" input_text)
string(REPLACE
  "#5=CARTESIAN_POINT('',(0.00003,0.,0.));"
  "#5=CARTESIAN_POINT('',(0.00003,0.00002,0.));"
  reject_text "${input_text}")
file(WRITE "${REJECT_INPUT}" "${reject_text}")
execute_process(
  COMMAND "${AP214_G}" -D --report "${REJECT_REPORT}" "${REJECT_INPUT}"
  RESULT_VARIABLE reject_result
  OUTPUT_VARIABLE reject_output
  ERROR_VARIABLE reject_error
)
if(reject_result EQUAL 0)
  message(FATAL_ERROR "safe mode unexpectedly extended the NURBS to an off-continuation vertex\n${reject_output}\n${reject_error}")
endif()
file(READ "${REJECT_REPORT}" reject_report_text)
string(FIND "${reject_report_text}" "do not define a usable source interval" reject_found)
if(reject_found EQUAL -1)
  message(FATAL_ERROR "off-continuation report did not retain the source interval defect:\n${reject_report_text}")
endif()
