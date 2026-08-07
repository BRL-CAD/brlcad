if(NOT DEFINED STEP_G OR NOT DEFINED UNSUPPORTED_INPUT OR
   NOT DEFINED MALFORMED_INPUT OR
   NOT DEFINED EMPTY_INPUT OR
   NOT DEFINED PARTIAL_INPUT OR NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR
    "STEP_G, UNSUPPORTED_INPUT, MALFORMED_INPUT, EMPTY_INPUT, PARTIAL_INPUT, and OUTPUT_DIR are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

set(unsupported_output "${OUTPUT_DIR}/ap203_unsupported_geometry.g")
set(unsupported_report "${OUTPUT_DIR}/ap203_unsupported_geometry.json")
set(unsupported_strict_output "${OUTPUT_DIR}/ap203_unsupported_geometry_strict.g")
set(unsupported_strict_report "${OUTPUT_DIR}/ap203_unsupported_geometry_strict.json")
set(malformed_output "${OUTPUT_DIR}/ap203_malformed_representation.g")
set(malformed_report "${OUTPUT_DIR}/ap203_malformed_representation.json")
set(empty_output "${OUTPUT_DIR}/ap203_non_geometric_representation.g")
set(empty_report "${OUTPUT_DIR}/ap203_non_geometric_representation.json")
set(partial_output "${OUTPUT_DIR}/ap203_partial_coverage.g")
set(partial_report "${OUTPUT_DIR}/ap203_partial_coverage.json")
set(partial_strict_output "${OUTPUT_DIR}/ap203_partial_coverage_strict.g")
set(partial_strict_report "${OUTPUT_DIR}/ap203_partial_coverage_strict.json")
set(partial_fixture "${OUTPUT_DIR}/ap203_partial_coverage.stp")
file(REMOVE
  "${unsupported_output}" "${unsupported_report}"
  "${unsupported_strict_output}" "${unsupported_strict_report}"
  "${malformed_output}" "${malformed_report}"
  "${empty_output}" "${empty_report}"
  "${partial_fixture}"
  "${partial_output}" "${partial_report}"
  "${partial_strict_output}" "${partial_strict_report}")

# Unsupported-only input is a failed conversion, not a successful empty
# product.  It must identify the exact product-bound entity and publish no DB.
execute_process(
  COMMAND "${STEP_G}" -o "${unsupported_output}" --report "${unsupported_report}"
    "${UNSUPPORTED_INPUT}"
  RESULT_VARIABLE unsupported_result
  OUTPUT_VARIABLE unsupported_log
  ERROR_VARIABLE unsupported_error
)
if(NOT unsupported_result EQUAL 3 OR EXISTS "${unsupported_output}")
  message(FATAL_ERROR
    "unsupported-only import returned ${unsupported_result} or published output:\n"
    "${unsupported_log}${unsupported_error}")
endif()
file(READ "${unsupported_report}" unsupported_text)
foreach(expected
    "\"exit_status\":3"
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1,\"outcome\":\"failed\""
    "\"entity_id\":14,\"product_id\":12,\"type\":\"SHAPE_REPRESENTATION\",\"status\":\"unsupported\""
    "\"entity_id\":15,\"type\":\"CARTESIAN_POINT\",\"status\":\"unsupported\""
    "no importer is registered for this product-bound representation item type")
  require_text("${unsupported_text}" "${expected}" "unsupported-only report")
endforeach()

# A product containing only a placement is intentionally non-geometric, not
# unsupported or failed.  Status 6 exposes that distinct empty outcome.
execute_process(
  COMMAND "${STEP_G}" -o "${empty_output}" --report "${empty_report}"
    "${EMPTY_INPUT}"
  RESULT_VARIABLE empty_result
  OUTPUT_VARIABLE empty_log
  ERROR_VARIABLE empty_error
)
if(NOT empty_result EQUAL 6 OR EXISTS "${empty_output}")
  message(FATAL_ERROR
    "non-geometric import returned ${empty_result} or published output:\n"
    "${empty_log}${empty_error}")
endif()
file(READ "${empty_report}" empty_text)
foreach(expected
    "\"exit_status\":6"
    "\"geometry_attempted\":0,\"geometry_written\":0,\"geometry_skipped\":0,\"outcome\":\"empty\""
    "\"entity_id\":14,\"product_id\":12,\"type\":\"SHAPE_REPRESENTATION\",\"status\":\"intentionally_non_geometric\""
    "\"entity_id\":15,\"type\":\"AXIS2_PLACEMENT_3D\",\"status\":\"intentionally_non_geometric\"")
  require_text("${empty_text}" "${expected}" "non-geometric report")
endforeach()

execute_process(
  COMMAND "${STEP_G}" --strict -o "${unsupported_strict_output}"
    --report "${unsupported_strict_report}" "${UNSUPPORTED_INPUT}"
  RESULT_VARIABLE unsupported_strict_result
  OUTPUT_VARIABLE unsupported_strict_log
  ERROR_VARIABLE unsupported_strict_error
)
if(NOT unsupported_strict_result EQUAL 3 OR EXISTS "${unsupported_strict_output}")
  message(FATAL_ERROR
    "strict unsupported-only import returned ${unsupported_strict_result} or published output:\n"
    "${unsupported_strict_log}${unsupported_strict_error}")
endif()
file(READ "${unsupported_strict_report}" unsupported_strict_text)
require_text("${unsupported_strict_text}" "\"strict\":true"
  "strict unsupported-only report")

execute_process(
  COMMAND "${STEP_G}" -o "${malformed_output}" --report "${malformed_report}"
    "${MALFORMED_INPUT}"
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_log
  ERROR_VARIABLE malformed_error
)
if(NOT malformed_result EQUAL 3 OR EXISTS "${malformed_output}")
  message(FATAL_ERROR
    "malformed representation import returned ${malformed_result} or published output:\n"
    "${malformed_log}${malformed_error}")
endif()
file(READ "${malformed_report}" malformed_text)
foreach(expected
    "\"entity_id\":9,\"product_id\":7,\"type\":\"SHAPE_REPRESENTATION\",\"status\":\"malformed\""
    "representation has no resolvable context_of_items"
    "\"entity_id\":10,\"type\":\"CARTESIAN_POINT\",\"status\":\"unsupported\"")
  require_text("${malformed_text}" "${expected}" "malformed representation report")
endforeach()

# A permissive mixed import publishes its usable geometry with status 1 and a
# complete omission record.  Strict mode must not publish that same partial DB.
# The source fixture's relationship-backed Cartesian point set is supported.
# Add a POINT_ON_CURVE set to retain coverage of the intentional exact-path
# limitation: implicit points require curve evaluation before they can be
# emitted as BRL-CAD point geometry.
file(READ "${PARTIAL_INPUT}" partial_fixture_text)
string(CONCAT partial_branch
  "#138=POINT_ON_CURVE('implicit point',#12,0.5);\n"
  "#139=GEOMETRIC_SET('unsupported implicit point',(#138));\n"
  "#140=GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION('',(#139),#115);\n"
  "#141=SHAPE_REPRESENTATION_RELATIONSHIP('implicit point','',#128,#140);\n"
  "ENDSEC;\nEND-ISO-10303-21;")
string(REPLACE "ENDSEC;\nEND-ISO-10303-21;" "${partial_branch}"
  partial_fixture_text "${partial_fixture_text}")
file(WRITE "${partial_fixture}" "${partial_fixture_text}")
execute_process(
  COMMAND "${STEP_G}" -o "${partial_output}" --report "${partial_report}"
    "${partial_fixture}"
  RESULT_VARIABLE partial_result
  OUTPUT_VARIABLE partial_log
  ERROR_VARIABLE partial_error
)
if(NOT partial_result EQUAL 1 OR NOT EXISTS "${partial_output}")
  message(FATAL_ERROR
    "permissive partial import returned ${partial_result} or omitted output:\n"
    "${partial_log}${partial_error}")
endif()
file(READ "${partial_report}" partial_text)
foreach(expected
    "\"outcome\":\"partial\""
    "\"geometry_attempted\":4,\"geometry_written\":3,\"geometry_skipped\":1"
    "\"entity_id\":140,\"product_id\":124,\"type\":\"GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION\",\"status\":\"unsupported\""
    "\"entity_id\":139,\"type\":\"GEOMETRIC_SET\",\"status\":\"unsupported\"")
  require_text("${partial_text}" "${expected}" "partial import report")
endforeach()

execute_process(
  COMMAND "${STEP_G}" --strict -o "${partial_strict_output}"
    --report "${partial_strict_report}" "${partial_fixture}"
  RESULT_VARIABLE partial_strict_result
  OUTPUT_VARIABLE partial_strict_log
  ERROR_VARIABLE partial_strict_error
)
if(NOT partial_strict_result EQUAL 4 OR EXISTS "${partial_strict_output}")
  message(FATAL_ERROR
    "strict partial import returned ${partial_strict_result} or published output:\n"
    "${partial_strict_log}${partial_strict_error}")
endif()
file(READ "${partial_strict_report}" partial_strict_text)
foreach(expected "\"strict\":true" "\"outcome\":\"failed\""
    "\"entity_id\":139,\"type\":\"GEOMETRIC_SET\",\"status\":\"unsupported\"")
  require_text("${partial_strict_text}" "${expected}" "strict partial report")
endforeach()
