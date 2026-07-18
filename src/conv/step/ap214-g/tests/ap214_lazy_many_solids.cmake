if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "AP214_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${AP214_G}" -D -j 1 --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "bounded lazy representation import failed (${result}):\n${output}\n${error}")
endif()

file(READ "${REPORT}" report_text)
if(NOT report_text MATCHES
    "\"geometry_attempted\":64,\"geometry_written\":64,\"geometry_skipped\":0")
  message(FATAL_ERROR "bounded lazy fixture did not convert all 64 exact solids:\n${report_text}")
endif()
string(REGEX MATCH "\"indexed_instances\":([0-9]+)" indexed_match "${report_text}")
set(indexed "${CMAKE_MATCH_1}")
string(REGEX MATCH "\"high_water_instances\":([0-9]+)" high_water_match "${report_text}")
set(high_water "${CMAKE_MATCH_1}")
if(indexed STREQUAL "" OR high_water STREQUAL "")
  message(FATAL_ERROR "bounded lazy fixture report lacks cache statistics:\n${report_text}")
endif()
math(EXPR representation_wide_threshold "${indexed} - 32")
if(high_water GREATER_EQUAL representation_wide_threshold)
  message(FATAL_ERROR
    "cache high-water ${high_water} is representation-wide for ${indexed} indexed instances")
endif()
foreach(cache_field IN ITEMS loaded_instances pinned_instances active_batches bytes)
  if(NOT report_text MATCHES "\"${cache_field}\":0")
    message(FATAL_ERROR "bounded lazy fixture retained ${cache_field}:\n${report_text}")
  endif()
endforeach()
