if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()
file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Appendix J multiple-SDR import returned ${result}:\n${log}${error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"products\":1"
    "\"SHAPE_ASPECT\":1"
    "\"PROPERTY_DEFINITION\":1"
    "\"SHAPE_DEFINITION_REPRESENTATION\":2"
    "\"SHAPE_REPRESENTATION_RELATIONSHIP\":1"
    "\"GEOMETRIC_SET\":1"
    "\"geometry_attempted\":3,\"geometry_written\":3,\"geometry_skipped\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Appendix J report omits ${expected}:\n${report_text}")
  endif()
endforeach()
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "tree Appendix_J_Part"
  OUTPUT_VARIABLE tree_output ERROR_VARIABLE tree_error)
set(tree_text "${tree_output}${tree_error}")
string(REGEX MATCHALL "Appendix_J_Part_wire[^ /]*\\.s" wire_solids "${tree_text}")
list(LENGTH wire_solids wire_count)
if(NOT wire_count EQUAL 2)
  message(FATAL_ERROR "Appendix J product does not contain both shape representations:\n${tree_text}")
endif()
string(FIND "${tree_text}" "\tu Appendix_J_Part_item\n" point_set)
if(point_set EQUAL -1)
  message(FATAL_ERROR "Appendix J product does not contain the relationship-backed point set:\n${tree_text}")
endif()
