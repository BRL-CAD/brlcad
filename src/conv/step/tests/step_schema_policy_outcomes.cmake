if(NOT DEFINED STEP_G OR NOT DEFINED UNSUPPORTED_TEMPLATE OR
   NOT DEFINED MALFORMED_TEMPLATE OR NOT DEFINED EMPTY_TEMPLATE OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED SCHEMA OR NOT DEFINED SCHEMA_LABEL OR
   NOT DEFINED APPLICATION OR NOT DEFINED ASSOCIATION)
  message(FATAL_ERROR "all schema policy outcome fixture parameters are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

set(PRODUCT_ID unsupported_point)
set(PRODUCT_NAME "Unsupported Point")
set(FILE_NAME "${SCHEMA_LABEL}_unsupported_geometry.stp")
set(unsupported_input "${OUTPUT_DIR}/${FILE_NAME}")
configure_file("${UNSUPPORTED_TEMPLATE}" "${unsupported_input}" @ONLY)

set(PRODUCT_ID malformed_context)
set(PRODUCT_NAME "Malformed Context")
set(FILE_NAME "${SCHEMA_LABEL}_malformed_representation.stp")
set(malformed_input "${OUTPUT_DIR}/${FILE_NAME}")
configure_file("${MALFORMED_TEMPLATE}" "${malformed_input}" @ONLY)

set(PRODUCT_ID placement_only)
set(PRODUCT_NAME "Placement Only")
set(FILE_NAME "${SCHEMA_LABEL}_non_geometric_representation.stp")
set(empty_input "${OUTPUT_DIR}/${FILE_NAME}")
configure_file("${EMPTY_TEMPLATE}" "${empty_input}" @ONLY)

set(unsupported_output "${OUTPUT_DIR}/${SCHEMA_LABEL}_unsupported_geometry.g")
set(unsupported_report "${OUTPUT_DIR}/${SCHEMA_LABEL}_unsupported_geometry.json")
set(unsupported_strict_report "${OUTPUT_DIR}/${SCHEMA_LABEL}_unsupported_geometry_strict.json")
set(malformed_output "${OUTPUT_DIR}/${SCHEMA_LABEL}_malformed_representation.g")
set(malformed_report "${OUTPUT_DIR}/${SCHEMA_LABEL}_malformed_representation.json")
set(empty_output "${OUTPUT_DIR}/${SCHEMA_LABEL}_non_geometric_representation.g")
set(empty_report "${OUTPUT_DIR}/${SCHEMA_LABEL}_non_geometric_representation.json")
file(REMOVE
  "${unsupported_output}" "${unsupported_report}" "${unsupported_strict_report}"
  "${malformed_output}" "${malformed_report}" "${empty_output}" "${empty_report}")

# Unsupported-only input is a failed conversion and must identify the exact
# product-bound representation and item that could not be converted.
execute_process(
  COMMAND "${STEP_G}" -o "${unsupported_output}"
    --report "${unsupported_report}" "${unsupported_input}"
  RESULT_VARIABLE unsupported_result
  OUTPUT_VARIABLE unsupported_log
  ERROR_VARIABLE unsupported_error
)
if(NOT unsupported_result EQUAL 3 OR EXISTS "${unsupported_output}")
  message(FATAL_ERROR
    "${SCHEMA_LABEL} unsupported-only import returned ${unsupported_result} or published output:\n"
    "${unsupported_log}${unsupported_error}")
endif()
file(READ "${unsupported_report}" unsupported_text)
foreach(expected
    "\"exit_status\":3"
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1,\"outcome\":\"failed\""
    "\"entity_id\":14,\"product_id\":12,\"type\":\"SHAPE_REPRESENTATION\",\"status\":\"unsupported\""
    "\"entity_id\":15,\"type\":\"CARTESIAN_POINT\",\"status\":\"unsupported\""
    "no importer is registered for this product-bound representation item type")
  require_text("${unsupported_text}" "${expected}" "${SCHEMA_LABEL} unsupported report")
endforeach()

execute_process(
  COMMAND "${STEP_G}" --strict -D --report "${unsupported_strict_report}"
    "${unsupported_input}"
  RESULT_VARIABLE unsupported_strict_result
  OUTPUT_VARIABLE unsupported_strict_log
  ERROR_VARIABLE unsupported_strict_error
)
if(NOT unsupported_strict_result EQUAL 3)
  message(FATAL_ERROR
    "${SCHEMA_LABEL} strict unsupported import returned ${unsupported_strict_result}:\n"
    "${unsupported_strict_log}${unsupported_strict_error}")
endif()
file(READ "${unsupported_strict_report}" unsupported_strict_text)
require_text("${unsupported_strict_text}" "\"strict\":true"
  "${SCHEMA_LABEL} strict unsupported report")

# A placement-only product is intentionally non-geometric and gets the
# distinct empty status rather than being mislabeled unsupported.
execute_process(
  COMMAND "${STEP_G}" -o "${empty_output}" --report "${empty_report}" "${empty_input}"
  RESULT_VARIABLE empty_result
  OUTPUT_VARIABLE empty_log
  ERROR_VARIABLE empty_error
)
if(NOT empty_result EQUAL 6 OR EXISTS "${empty_output}")
  message(FATAL_ERROR
    "${SCHEMA_LABEL} non-geometric import returned ${empty_result} or published output:\n"
    "${empty_log}${empty_error}")
endif()
file(READ "${empty_report}" empty_text)
foreach(expected
    "\"exit_status\":6"
    "\"geometry_attempted\":0,\"geometry_written\":0,\"geometry_skipped\":0,\"outcome\":\"empty\""
    "\"entity_id\":14,\"product_id\":12,\"type\":\"SHAPE_REPRESENTATION\",\"status\":\"intentionally_non_geometric\""
    "\"entity_id\":15,\"type\":\"AXIS2_PLACEMENT_3D\",\"status\":\"intentionally_non_geometric\"")
  require_text("${empty_text}" "${expected}" "${SCHEMA_LABEL} non-geometric report")
endforeach()

# An unresolved context is malformed, independently of the unsupported point
# item it happens to contain.
execute_process(
  COMMAND "${STEP_G}" -o "${malformed_output}"
    --report "${malformed_report}" "${malformed_input}"
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_log
  ERROR_VARIABLE malformed_error
)
if(NOT malformed_result EQUAL 3 OR EXISTS "${malformed_output}")
  message(FATAL_ERROR
    "${SCHEMA_LABEL} malformed import returned ${malformed_result} or published output:\n"
    "${malformed_log}${malformed_error}")
endif()
file(READ "${malformed_report}" malformed_text)
foreach(expected
    "\"entity_id\":9,\"product_id\":7,\"type\":\"SHAPE_REPRESENTATION\",\"status\":\"malformed\""
    "representation has no resolvable context_of_items"
    "\"entity_id\":10,\"type\":\"CARTESIAN_POINT\",\"status\":\"unsupported\"")
  require_text("${malformed_text}" "${expected}" "${SCHEMA_LABEL} malformed report")
endforeach()
