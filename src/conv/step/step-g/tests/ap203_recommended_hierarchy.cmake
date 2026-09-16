if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()
file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "referenced hierarchy import returned ${result}:\n${log}${error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"products\":3"
    "\"occurrences\":2"
    "\"shape_method\":\"referenced\""
    "\"reference_designator\":\"SUB-1\""
    "\"reference_designator\":\"LEAF-1\""
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "referenced hierarchy report omits ${expected}:\n${report_text}")
  endif()
endforeach()
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "tree Referenced_Root"
  OUTPUT_VARIABLE tree_output ERROR_VARIABLE tree_error)
set(tree_text "${tree_output}${tree_error}")
foreach(expected "Referenced_Subassembly" "Referenced_Leaf")
  string(FIND "${tree_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "referenced hierarchy tree omits ${expected}:\n${tree_text}")
  endif()
endforeach()
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "l Referenced_Root"
  OUTPUT_VARIABLE root_matrix_output ERROR_VARIABLE root_matrix_error)
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "l Referenced_Subassembly"
  OUTPUT_VARIABLE sub_matrix_output ERROR_VARIABLE sub_matrix_error)
set(matrix_text
  "${root_matrix_output}${root_matrix_error}${sub_matrix_output}${sub_matrix_error}")
foreach(expected
    "Referenced_Subassembly [100, 0, 0]"
    "Referenced_Leaf [0, 50, 0]")
  string(FIND "${matrix_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "referenced hierarchy placement omits ${expected}:\n${matrix_text}")
  endif()
endforeach()
