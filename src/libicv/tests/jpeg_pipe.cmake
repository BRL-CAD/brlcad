foreach(mode --stdin --stdin-bad)
  execute_process(COMMAND "${JPEG_TEST}" --emit COMMAND "${JPEG_TEST}" ${mode}
    RESULTS_VARIABLE statuses)
  if(NOT "${statuses}" STREQUAL "0;0")
    message(FATAL_ERROR "JPEG pipe profile validation failed: ${mode}: ${statuses}")
  endif()
endforeach()
