if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED MULTI_INPUT OR
   NOT DEFINED OUTPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "STEP_G, input, generated input, output, and report are required")
endif()

file(READ "${INPUT}" fixture)
set(primary "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));")
string(CONCAT multiple
  "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN',"
  "'GEOMETRIC_VALIDATION_PROPERTIES_MIM','SHAPE_APPEARANCE_LAYER_MIM'));")
string(FIND "${fixture}" "${primary}" schema_offset)
if(schema_offset EQUAL -1)
  message(FATAL_ERROR "AP203 fixture no longer has the expected FILE_SCHEMA")
endif()
string(REPLACE "${primary}" "${multiple}" multi_fixture "${fixture}")
file(WRITE "${MULTI_INPUT}" "${multi_fixture}")

file(REMOVE "${OUTPUT}" "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -j 8 -O "${OUTPUT}" --report "${REPORT}" "${MULTI_INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE standard_output
  ERROR_VARIABLE standard_error
)
if(NOT result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "multi-schema AP203 import returned ${result} or omitted output:\n"
    "${standard_output}${standard_error}")
endif()

string(CONCAT converter_output "${standard_output}" "${standard_error}")
string(CONCAT expected_dispatch
  "accepting legacy/interim FILE_SCHEMA identifier or profile "
  "as ap203e2")
if(NOT converter_output MATCHES "${expected_dispatch}")
  message(FATAL_ERROR
    "interim AP203 companion-profile dispatch warning is missing:\n${converter_output}")
endif()
file(READ "${REPORT}" report)
if(NOT report MATCHES
    "\"geometry_attempted\":3,\"geometry_written\":3,\"geometry_skipped\":0")
  message(FATAL_ERROR "multi-schema AP203 coverage is incomplete:\n${report}")
endif()
