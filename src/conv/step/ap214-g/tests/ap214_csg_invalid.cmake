if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
   NOT DEFINED OUTPUT OR NOT DEFINED STRICT_REPORT OR NOT DEFINED STRICT_OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, OUTPUT, STRICT_REPORT, and STRICT_OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}" "${STRICT_REPORT}" "${STRICT_OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 1)
  message(FATAL_ERROR "partial ap214-g import returned ${import_result}, expected 1\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"exit_status\":1"
    "\"geometry_attempted\":2"
    "\"geometry_written\":1"
    "\"geometry_skipped\":1"
    "unsupported or invalid exact CSG tree")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "partial report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get Partial_CSG_csg_primitive.s
  OUTPUT_VARIABLE valid_output
  ERROR_VARIABLE valid_error
)
set(valid_text "${valid_output}\n${valid_error}")
if(NOT valid_text MATCHES "ell V")
  message(FATAL_ERROR "valid CSG primitive was not retained\n${valid_output}\n${valid_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get Partial_CSG_csg_primitive_step11.s
  OUTPUT_VARIABLE invalid_output
  ERROR_VARIABLE invalid_error
)
set(invalid_text "${invalid_output}\n${invalid_error}")
if(NOT invalid_text MATCHES "not found")
  message(FATAL_ERROR "invalid CSG primitive leaked into partial output\n${invalid_output}\n${invalid_error}")
endif()

execute_process(
  COMMAND "${AP214_G}" -O "${STRICT_OUTPUT}" --strict --report "${STRICT_REPORT}" "${INPUT}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_output
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 3)
  message(FATAL_ERROR "strict ap214-g import returned ${strict_result}, expected 3\n${strict_output}\n${strict_error}")
endif()
if(EXISTS "${STRICT_OUTPUT}")
  message(FATAL_ERROR "strict partial import published an output database")
endif()
file(READ "${STRICT_REPORT}" strict_report_text)
string(FIND "${strict_report_text}" "\"exit_status\":3" strict_status)
if(strict_status EQUAL -1)
  message(FATAL_ERROR "strict report does not record exit status 3:\n${strict_report_text}")
endif()
