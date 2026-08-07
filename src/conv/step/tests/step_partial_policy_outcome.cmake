if(NOT DEFINED STEP_G OR NOT DEFINED TEMPLATE OR NOT DEFINED INPUT OR
   NOT DEFINED OUTPUT OR NOT DEFINED REPORT OR NOT DEFINED STRICT_OUTPUT OR
   NOT DEFINED STRICT_REPORT OR NOT DEFINED SCHEMA OR NOT DEFINED SCHEMA_LABEL OR
   NOT DEFINED FILE_NAME OR NOT DEFINED SOLID_NAME OR NOT DEFINED TOLERANCE OR
   NOT DEFINED APPLICATION OR NOT DEFINED PRODUCT_ID OR NOT DEFINED PRODUCT_NAME OR
   NOT DEFINED ASSOCIATION)
  message(FATAL_ERROR "all partial policy fixture parameters are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

set(base_input "${INPUT}.base")
configure_file("${TEMPLATE}" "${base_input}" @ONLY)
file(READ "${base_input}" partial_text)
string(CONCAT partial_branch
  "#130=POINT_ON_CURVE('implicit point',#12,0.5);\n"
  "#131=GEOMETRIC_SET('unsupported branch',(#130));\n"
  "#132=GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION('',(#131),#115);\n"
  "#133=SHAPE_REPRESENTATION_RELATIONSHIP('unsupported branch','',#128,#132);\n"
  "ENDSEC;\nEND-ISO-10303-21;")
string(REPLACE "ENDSEC;\nEND-ISO-10303-21;" "${partial_branch}" partial_text "${partial_text}")
file(WRITE "${INPUT}" "${partial_text}")
file(REMOVE "${OUTPUT}" "${REPORT}" "${STRICT_OUTPUT}" "${STRICT_REPORT}")

# Permissive mode publishes the exact solid, returns the partial status, and
# accounts for the unsupported representation branch and its item.
execute_process(
  COMMAND "${STEP_G}" -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE partial_result
  OUTPUT_VARIABLE partial_log
  ERROR_VARIABLE partial_error
)
if(NOT partial_result EQUAL 1 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "${SCHEMA_LABEL} permissive partial import returned ${partial_result} or omitted output:\n"
    "${partial_log}${partial_error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":2,\"geometry_written\":1,\"geometry_skipped\":1,\"outcome\":\"partial\""
    "\"entity_id\":132,\"product_id\":124,\"type\":\"GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION\",\"status\":\"unsupported\""
    "\"entity_id\":131,\"type\":\"GEOMETRIC_SET\",\"status\":\"unsupported\""
    "\"invalid_breps\":0"
    "\"tolerance_mm\":${TOLERANCE}")
  require_text("${report_text}" "${expected}" "${SCHEMA_LABEL} partial report")
endforeach()

# Strict mode processes the file for a complete report but never publishes the
# partially converted database.
execute_process(
  COMMAND "${STEP_G}" --strict -o "${STRICT_OUTPUT}"
    --report "${STRICT_REPORT}" "${INPUT}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_log
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 4 OR EXISTS "${STRICT_OUTPUT}")
  message(FATAL_ERROR
    "${SCHEMA_LABEL} strict partial import returned ${strict_result} or published output:\n"
    "${strict_log}${strict_error}")
endif()
file(READ "${STRICT_REPORT}" strict_text)
foreach(expected
    "\"strict\":true"
    "\"outcome\":\"failed\""
    "\"entity_id\":131,\"type\":\"GEOMETRIC_SET\",\"status\":\"unsupported\"")
  require_text("${strict_text}" "${expected}" "${SCHEMA_LABEL} strict partial report")
endforeach()
