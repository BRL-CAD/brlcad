if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 3)
  message(FATAL_ERROR "cyclic replica returned ${import_result}, expected 3\n${import_output}\n${import_error}")
endif()
if(EXISTS "${OUTPUT}")
  message(FATAL_ERROR "cyclic replica unexpectedly produced an output database")
endif()
set(import_text "${import_output}\n${import_error}")
if(NOT import_text MATCHES "cyclic dependency encountered while materializing instance")
  message(FATAL_ERROR "cyclic replica diagnostic was not emitted\n${import_text}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"exit_status\":3"
    "\"geometry_written\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
