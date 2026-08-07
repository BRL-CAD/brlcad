if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED TEMPLATE OR
   NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED REPORT OR
   NOT DEFINED SCHEMA OR NOT DEFINED SCHEMA_LABEL OR NOT DEFINED FILE_NAME OR
   NOT DEFINED TOLERANCE OR NOT DEFINED APPLICATION OR NOT DEFINED PRODUCT_ID OR
   NOT DEFINED PRODUCT_NAME OR NOT DEFINED ASSOCIATION OR NOT DEFINED TOP)
  message(FATAL_ERROR "all wireframe fixture parameters are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

configure_file("${TEMPLATE}" "${INPUT}" @ONLY)
file(REMOVE "${OUTPUT}" "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" --strict -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "strict ${SCHEMA_LABEL} wireframe import returned ${import_result} or omitted output:\n"
    "${import_output}${import_error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"GEOMETRICALLY_BOUNDED_WIREFRAME_SHAPE_REPRESENTATION\":1"
    "\"GEOMETRIC_SET\":1"
    "\"B_SPLINE_CURVE_WITH_KNOTS\":1"
    "\"CURVE_REPLICA\":1"
    "\"${ASSOCIATION}\":1"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"invalid_breps\":0"
    "\"tolerance_mm\":${TOLERANCE}"
    "\"skipped_items\":[]")
  require_text("${report_text}" "${expected}" "${SCHEMA_LABEL} wireframe report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep "${TOP}_item.s" info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}${brep_error}")
if(NOT brep_result EQUAL 0)
  message(FATAL_ERROR "unable to inspect ${TOP}_item.s:\n${brep_text}")
endif()
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+0" OR
   NOT brep_text MATCHES "edges:[ ]+2" OR
   NOT brep_text MATCHES "3d curve:[ ]+2" OR
   NOT brep_text MATCHES "vertices:[ ]+4")
  message(FATAL_ERROR "${SCHEMA_LABEL} wire BREP is wrong:\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep "${TOP}_item.s" info C3 1
  OUTPUT_VARIABLE replica_output
  ERROR_VARIABLE replica_error
)
set(replica_text "${replica_output}${replica_error}")
if(NOT replica_text MATCHES "CV\\[[ ]*0\\] \\(30, 0, 0\\)" OR
   NOT replica_text MATCHES "CV\\[[ ]*3\\] \\(22, 40, 20\\)")
  message(FATAL_ERROR "${SCHEMA_LABEL} replica transform is wrong:\n${replica_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" get "${TOP}_item"
  OUTPUT_VARIABLE comb_output
  ERROR_VARIABLE comb_error
)
require_text("${comb_output}${comb_error}" "comb region no"
  "${SCHEMA_LABEL} zero-thickness wireframe wrapper")
