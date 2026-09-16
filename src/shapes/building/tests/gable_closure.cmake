if(NOT DEFINED NIRT_EXECUTABLE OR NOT DEFINED BUILDING_DATABASE)
  message(FATAL_ERROR "NIRT_EXECUTABLE and BUILDING_DATABASE are required")
endif()

execute_process(
  COMMAND "${NIRT_EXECUTABLE}"
    -e "xyz -1000 4500 7000; dir 1 0 0; s; q"
    "${BUILDING_DATABASE}" house
  RESULT_VARIABLE nirt_result
  OUTPUT_VARIABLE nirt_output
  ERROR_VARIABLE nirt_error
  )
if(NOT nirt_result EQUAL 0)
  message(FATAL_ERROR "nirt failed (${nirt_result}):\n${nirt_output}\n${nirt_error}")
endif()

string(REGEX MATCHALL "house_walls\\.r[ \t]+\\(" closure_hits "${nirt_output}")
list(LENGTH closure_hits closure_count)
if(NOT closure_count EQUAL 2)
  message(FATAL_ERROR
    "Expected the ray to hit both gable closures, found ${closure_count}:\n${nirt_output}"
    )
endif()
