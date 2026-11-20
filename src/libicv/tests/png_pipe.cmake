foreach(mode --stdin --stdin-bad)
  execute_process(COMMAND "${PNG_TEST}" --emit COMMAND "${PNG_TEST}" ${mode}
    RESULTS_VARIABLE statuses)
  if(NOT "${statuses}" STREQUAL "0;0")
    message(FATAL_ERROR "PNG pipe profile validation failed: ${mode}: ${statuses}")
  endif()
endforeach()

execute_process(COMMAND "${PNG_TEST}" --emit-observed COMMAND "${PNG_TEST}" --stdin-observed
  RESULTS_VARIABLE statuses)
if(NOT "${statuses}" STREQUAL "0;0")
  message(FATAL_ERROR "PNG pipe lost contextual observations: ${statuses}")
endif()
