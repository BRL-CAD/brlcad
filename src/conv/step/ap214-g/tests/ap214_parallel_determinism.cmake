if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT_SERIAL OR
    NOT DEFINED OUTPUT_PARALLEL OR NOT DEFINED REPORT_SERIAL OR
    NOT DEFINED REPORT_PARALLEL OR NOT DEFINED EXPECTED_GEOMETRY)
  message(FATAL_ERROR "AP214_G, INPUT, outputs, reports, and EXPECTED_GEOMETRY are required")
endif()

file(REMOVE "${OUTPUT_SERIAL}" "${OUTPUT_PARALLEL}"
  "${REPORT_SERIAL}" "${REPORT_PARALLEL}")
execute_process(
  COMMAND "${AP214_G}" -j 1 -O "${OUTPUT_SERIAL}" --report "${REPORT_SERIAL}" "${INPUT}"
  RESULT_VARIABLE serial_result
  OUTPUT_VARIABLE serial_stdout
  ERROR_VARIABLE serial_stderr
)
execute_process(
  COMMAND "${AP214_G}" -j 4 -O "${OUTPUT_PARALLEL}" --report "${REPORT_PARALLEL}" "${INPUT}"
  RESULT_VARIABLE parallel_result
  OUTPUT_VARIABLE parallel_stdout
  ERROR_VARIABLE parallel_stderr
)
if(NOT serial_result EQUAL 0 OR NOT parallel_result EQUAL 0)
  message(FATAL_ERROR
    "parallel determinism imports failed (${serial_result}, ${parallel_result})\n"
    "${serial_stdout}${serial_stderr}${parallel_stdout}${parallel_stderr}")
endif()

file(SHA256 "${OUTPUT_SERIAL}" serial_hash)
file(SHA256 "${OUTPUT_PARALLEL}" parallel_hash)
if(NOT serial_hash STREQUAL parallel_hash)
  message(FATAL_ERROR
    "-j 1 and -j 4 outputs differ: ${serial_hash} != ${parallel_hash}")
endif()

file(READ "${REPORT_SERIAL}" serial_report)
file(READ "${REPORT_PARALLEL}" parallel_report)
if(NOT serial_report MATCHES "\"requested_jobs\":1,\"effective_jobs\":1")
  message(FATAL_ERROR "serial report does not record one effective worker")
endif()
if(NOT parallel_report MATCHES "\"requested_jobs\":4,\"effective_jobs\":4")
  message(FATAL_ERROR "parallel report does not record four effective workers")
endif()
foreach(report IN ITEMS serial_report parallel_report)
  if(NOT "${${report}}" MATCHES "\"geometry_attempted\":${EXPECTED_GEOMETRY},\"geometry_written\":${EXPECTED_GEOMETRY},\"geometry_skipped\":0")
    message(FATAL_ERROR "${report} does not show ${EXPECTED_GEOMETRY} successful detached geometry jobs")
  endif()
  foreach(cache_field IN ITEMS loaded_instances pinned_instances active_batches)
    if(NOT "${${report}}" MATCHES "\"${cache_field}\":0")
      message(FATAL_ERROR "${report} retained STEPcode cache state in ${cache_field}")
    endif()
  endforeach()
  if(NOT "${${report}}" MATCHES "\"bytes\":0")
    message(FATAL_ERROR "${report} retained materialized source-record bytes")
  endif()
  if(NOT "${${report}}" MATCHES "\"high_water_bytes\":[1-9][0-9]*")
    message(FATAL_ERROR "${report} lacks a source-record byte high-water mark")
  endif()
endforeach()
