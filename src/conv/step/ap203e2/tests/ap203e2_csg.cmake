if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "step-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

foreach(unexpected
    "Factory Method not mapped"
    "no converter factory is registered for this entity type")
  string(FIND "${import_error}" "${unexpected}" unmapped_factory)
  if(NOT unmapped_factory EQUAL -1)
    message(FATAL_ERROR
      "AP203e2 associated-document product definition was not mapped:\n${import_error}")
  endif()
endforeach()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"unsupported_counts\":{}"
    "\"CSG_SOLID\":1"
    "\"DOCUMENT\":1"
    "\"PRODUCT_DEFINITION_WITH_ASSOCIATED_DOCUMENTS\":1")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" tree CSG_Product
  RESULT_VARIABLE tree_result
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
set(tree_text "${tree_output}\n${tree_error}")
if(NOT tree_result EQUAL 0 OR NOT tree_text MATCHES "CSG_Product_csg_item" OR
   NOT tree_text MATCHES "CSG_Product_csg_node_step84")
  message(FATAL_ERROR "native AP203e2 CSG hierarchy validation failed\n${tree_text}")
endif()

set(primitive_checks
  "CSG_Product_csg_primitive.s|arb8 V1"
  "CSG_Product_csg_primitive_step10.s|tgc V"
  "CSG_Product_csg_primitive_step13.s|ell V"
  "CSG_Product_csg_primitive_step62.s|tor V"
  "CSG_Product_csg_primitive_step66.s|arb8 V1"
  "CSG_Product_csg_primitive_step70.s|tgc V"
  "CSG_Product_csg_half.s|half N"
)
foreach(check IN LISTS primitive_checks)
  string(REPLACE "|" ";" fields "${check}")
  list(GET fields 0 object)
  list(GET fields 1 expected)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" db get "${object}"
    RESULT_VARIABLE get_result
    OUTPUT_VARIABLE get_output
    ERROR_VARIABLE get_error
  )
  set(get_text "${get_output}\n${get_error}")
  string(FIND "${get_text}" "${expected}" found)
  if(NOT get_result EQUAL 0 OR found EQUAL -1)
    message(FATAL_ERROR
      "AP203e2 primitive ${object} did not contain '${expected}':\n${get_text}")
  endif()
endforeach()
