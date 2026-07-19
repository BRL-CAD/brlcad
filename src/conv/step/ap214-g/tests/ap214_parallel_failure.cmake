if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -j 4 -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 1)
  message(FATAL_ERROR
    "partial parallel import returned ${import_result}, expected 1\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"requested_jobs\":4,\"effective_jobs\":4"
    "\"geometry_attempted\":2"
    "\"geometry_written\":1"
    "\"geometry_skipped\":1"
    "closed STEP BREP did not validate as a solid")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "parallel failure report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Parallel_Failure_item_step91.s info
  OUTPUT_VARIABLE valid_output
  ERROR_VARIABLE valid_error
)
set(valid_text "${valid_output}\n${valid_error}")
if(NOT valid_text MATCHES "Valid: YES, Solid: YES")
  message(FATAL_ERROR "valid worker result was not retained\n${valid_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get Parallel_Failure_item.s
  OUTPUT_VARIABLE invalid_output
  ERROR_VARIABLE invalid_error
)
set(invalid_text "${invalid_output}\n${invalid_error}")
if(NOT invalid_text MATCHES "not found")
  message(FATAL_ERROR "invalid worker result leaked into output\n${invalid_text}")
endif()
