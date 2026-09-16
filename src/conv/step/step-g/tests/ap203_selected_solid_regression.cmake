if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED ENTITY_IDS OR NOT DEFINED EXPECTED_ATTEMPTED)
  message(FATAL_ERROR
    "STEP_G, INPUT, REPORT, ENTITY_IDS, and EXPECTED_ATTEMPTED are required")
endif()

if(NOT DEFINED TEST_TIMEOUT)
  set(TEST_TIMEOUT 45)
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --entity "${ENTITY_IDS}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT ${TEST_TIMEOUT}
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "AP203 selected-solid regression failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":${EXPECTED_ATTEMPTED}"
    "\"geometry_written\":${EXPECTED_ATTEMPTED}"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

if(DEFINED EXPECTED_DIAGNOSTIC)
  string(FIND "${report_text}" "${EXPECTED_DIAGNOSTIC}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "report does not contain expected diagnostic '${EXPECTED_DIAGNOSTIC}':\n"
      "${report_text}")
  endif()
endif()
