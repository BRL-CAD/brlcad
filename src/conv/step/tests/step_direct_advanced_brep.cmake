if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED TEMPLATE OR
   NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED REPORT OR
   NOT DEFINED SCHEMA OR NOT DEFINED SCHEMA_LABEL OR NOT DEFINED FILE_NAME OR
   NOT DEFINED SOLID_NAME OR NOT DEFINED TOLERANCE OR NOT DEFINED APPLICATION OR
   NOT DEFINED PRODUCT_ID OR NOT DEFINED PRODUCT_NAME OR NOT DEFINED ASSOCIATION OR
   NOT DEFINED TOP)
  message(FATAL_ERROR "all direct advanced BREP fixture parameters are required")
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
    "strict ${SCHEMA_LABEL} direct advanced BREP import returned ${import_result} "
    "or omitted output:\n${import_output}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"exit_status\":0"
    "\"strict\":true"
    "\"ADVANCED_BREP_SHAPE_REPRESENTATION\":1"
    "\"ADVANCED_FACE\":4"
    "\"MANIFOLD_SOLID_BREP\":1"
    "\"${ASSOCIATION}\":1"
    "\"products\":1,\"occurrences\":0,\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"type\":\"MANIFOLD_SOLID_BREP\",\"status\":\"handled\""
    "\"tolerance_mm\":${TOLERANCE}"
    "\"skipped_items\":[]")
  require_text("${report_text}" "${expected}" "${SCHEMA_LABEL} direct BREP report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" tree "${TOP}"
  RESULT_VARIABLE tree_result
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
set(tree_text "${tree_output}${tree_error}")
if(NOT tree_result EQUAL 0)
  message(FATAL_ERROR "unable to inspect ${TOP}:\n${tree_text}")
endif()
require_text("${tree_text}" "${TOP}_item.s" "${SCHEMA_LABEL} direct BREP tree")

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get "${TOP}_item.s"
  RESULT_VARIABLE get_result
  OUTPUT_VARIABLE get_output
  ERROR_VARIABLE get_error
)
set(get_text "${get_output}${get_error}")
if(NOT get_result EQUAL 0)
  message(FATAL_ERROR "unable to inspect ${TOP}_item.s:\n${get_text}")
endif()
require_text("${get_text}" "brep" "${SCHEMA_LABEL} exact BREP object")
