if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --strict -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "strict AP242 CSG import returned ${import_result} or omitted output:\n"
    "${import_output}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"strict\":true"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"CSG_SOLID\":1"
    "\"ELLIPSOID\":1"
    "\"RECTANGULAR_PYRAMID\":1"
    "\"PROPERTY_DEFINITION_REPRESENTATION\":1"
    "\"type\":\"CSG_SOLID\",\"status\":\"handled\""
    "exact CSG tree converted successfully"
    "STEP::AP242::FILE_SCHEMA"
    "\"skipped_items\":[]")
  require_text("${report_text}" "${expected}" "AP242 CSG report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" tree AP242_CSG_Product
  RESULT_VARIABLE tree_result
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
set(tree_text "${tree_output}${tree_error}")
foreach(expected
    "AP242_CSG_Product_csg_node/"
    "AP242_CSG_Product_csg_primitive.s"
    "AP242_CSG_Product_csg_primitive_step11.s")
  require_text("${tree_text}" "${expected}" "AP242 native CSG tree")
endforeach()
if(NOT tree_result EQUAL 0)
  message(FATAL_ERROR "AP242 native CSG tree inspection failed:\n${tree_text}")
endif()

set(primitive_checks
  "AP242_CSG_Product_csg_primitive.s|arb8 V1 {0 0 0}|V5 {6 4 10}"
  "AP242_CSG_Product_csg_primitive_step11.s|ell V {20 3 4}|A {4 0 0}|B {0 6 0}|C {0 0 8}"
)
foreach(check IN LISTS primitive_checks)
  string(REPLACE "|" ";" fields "${check}")
  list(POP_FRONT fields object)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" db get "${object}"
    RESULT_VARIABLE get_result
    OUTPUT_VARIABLE get_output
    ERROR_VARIABLE get_error
  )
  set(get_text "${get_output}${get_error}")
  if(NOT get_result EQUAL 0)
    message(FATAL_ERROR "unable to inspect ${object}:\n${get_text}")
  endif()
  foreach(expected IN LISTS fields)
    require_text("${get_text}" "${expected}" "AP242 primitive ${object}")
  endforeach()
endforeach()
