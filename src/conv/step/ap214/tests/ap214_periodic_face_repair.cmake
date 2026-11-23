if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR
    NOT DEFINED ENTITY_ID OR NOT DEFINED RAW_INVALID_COUNT OR
    NOT DEFINED REPORT_SAFE OR NOT DEFINED REPORT_NONE OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, INPUT, ENTITY_ID, RAW_INVALID_COUNT, REPORT_SAFE, REPORT_NONE, and OUTPUT are required")
endif()

file(REMOVE "${REPORT_SAFE}" "${REPORT_NONE}" "${OUTPUT}")

execute_process(
  COMMAND "${STEP_G}" -D -j 2 --entity "${ENTITY_ID}" --repair none
    --report "${REPORT_NONE}" "${INPUT}"
  RESULT_VARIABLE unrepaired_result
  OUTPUT_VARIABLE unrepaired_output
  ERROR_VARIABLE unrepaired_error
  TIMEOUT 60
)
if(NOT unrepaired_result EQUAL RAW_INVALID_COUNT)
  message(FATAL_ERROR "unexpected unrepaired import status (${unrepaired_result}):\n${unrepaired_output}${unrepaired_error}")
endif()
file(READ "${REPORT_NONE}" unrepaired_report)
string(JSON unrepaired_invalid GET "${unrepaired_report}" validation invalid_breps_written)
if(NOT unrepaired_invalid EQUAL RAW_INVALID_COUNT)
  message(FATAL_ERROR "unexpected unrepaired invalid B-rep count:\n${unrepaired_report}")
endif()

execute_process(
  COMMAND "${STEP_G}" -j 2 --entity "${ENTITY_ID}" --strict --repair safe
    --report "${REPORT_SAFE}" "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE repaired_result
  OUTPUT_VARIABLE repaired_output
  ERROR_VARIABLE repaired_error
  TIMEOUT 60
)
if(NOT repaired_result EQUAL 0)
  message(FATAL_ERROR "safe periodic face repair failed (${repaired_result}):\n${repaired_output}${repaired_error}")
endif()
file(READ "${REPORT_SAFE}" repaired_report)
string(JSON repaired_geometry GET "${repaired_report}" coverage geometry_written)
string(JSON repaired_invalid GET "${repaired_report}" validation invalid_breps)
if(NOT repaired_geometry EQUAL 1 OR NOT repaired_invalid EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR "safe repair did not write a valid periodic face:\n${repaired_report}")
endif()

string(JSON direct_writes GET "${repaired_report}" performance stages geometry_write calls)
if(NOT direct_writes EQUAL 1 OR repaired_report MATCHES "\"spool_write\"")
  message(FATAL_ERROR "the single face should be written directly to its destination:\n${repaired_report}")
endif()

if(REQUIRE_AUTHORED_PCURVE)
  string(JSON reused_curves GET "${repaired_report}" performance stages authored_parameter_curve_reuse calls)
  if(NOT reused_curves EQUAL 4)
    message(FATAL_ERROR "the four verified supplied splines should be reused:\n${repaired_report}")
  endif()
endif()

if(EXPECTED_REPAIR AND NOT repaired_report MATCHES "${EXPECTED_REPAIR}")
  message(FATAL_ERROR "expected periodic repair was absent:\n${repaired_report}")
endif()
