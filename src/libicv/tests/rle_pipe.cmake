foreach(mode --stdin --stdin-bad)
  execute_process(COMMAND "${RLE_TEST}" --emit COMMAND "${RLE_TEST}" ${mode}
    RESULTS_VARIABLE statuses)
  if(NOT "${statuses}" STREQUAL "0;0")
    message(FATAL_ERROR "RLE pipe profile validation failed: ${mode}: ${statuses}")
  endif()
endforeach()
