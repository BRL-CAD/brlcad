if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D -j 2 --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
  TIMEOUT 60
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "mixed AP203 relationship import failed (${result})\n${stdout}\n${stderr}")
endif()
if(NOT EXISTS "${REPORT}")
  message(FATAL_ERROR "step-g did not write the mixed-relationship report")
endif()

file(READ "${REPORT}" report_text)
if(NOT report_text MATCHES
    "\"geometry_attempted\":2,\"geometry_written\":2,\"geometry_skipped\":0")
  message(FATAL_ERROR
    "mixed relationship fixture did not convert both lazy geometry roots:\n${report_text}")
endif()
string(REGEX MATCH "\"indexed_instances\":([0-9]+)" indexed_match "${report_text}")
set(indexed "${CMAKE_MATCH_1}")
string(REGEX MATCH "\"high_water_instances\":([0-9]+)" high_water_match "${report_text}")
set(high_water "${CMAKE_MATCH_1}")
if(indexed STREQUAL "" OR high_water STREQUAL "")
  message(FATAL_ERROR "mixed relationship report lacks lazy-cache statistics")
endif()
math(EXPR relationship_wide_threshold "${indexed} - 8")
if(high_water GREATER_EQUAL relationship_wide_threshold)
  message(FATAL_ERROR
    "cache high-water ${high_water} is relationship-wide for ${indexed} indexed instances")
endif()
foreach(cache_field IN ITEMS current_loaded_instances pinned_instances active_batches bytes)
  if(NOT report_text MATCHES "\"${cache_field}\":0")
    message(FATAL_ERROR "mixed relationship import retained ${cache_field}:\n${report_text}")
  endif()
endforeach()
