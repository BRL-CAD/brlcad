if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT_SERIAL OR
    NOT DEFINED OUTPUT_PARALLEL OR NOT DEFINED REPORT_SERIAL OR
    NOT DEFINED REPORT_PARALLEL OR NOT DEFINED EXPECTED_GEOMETRY)
  message(FATAL_ERROR "STEP_G, INPUT, outputs, reports, and EXPECTED_GEOMETRY are required")
endif()
if(NOT DEFINED PARALLEL_JOBS)
  set(PARALLEL_JOBS 8)
endif()
if(NOT DEFINED PARALLEL_REPEATS)
  set(PARALLEL_REPEATS 4)
endif()
if(NOT PARALLEL_JOBS MATCHES "^[2-9][0-9]*$" OR
   NOT PARALLEL_REPEATS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "PARALLEL_JOBS must be at least 2 and PARALLEL_REPEATS must be positive")
endif()

file(REMOVE "${OUTPUT_SERIAL}" "${REPORT_SERIAL}")
execute_process(
  COMMAND "${STEP_G}" -j 1 -O "${OUTPUT_SERIAL}" --report "${REPORT_SERIAL}" "${INPUT}"
  RESULT_VARIABLE serial_result
  OUTPUT_VARIABLE serial_stdout
  ERROR_VARIABLE serial_stderr
)
if(NOT serial_result EQUAL 0)
  message(FATAL_ERROR
    "serial determinism import failed (${serial_result})\n"
    "${serial_stdout}${serial_stderr}")
endif()

file(SHA256 "${OUTPUT_SERIAL}" serial_hash)
file(READ "${REPORT_SERIAL}" serial_report)
if(NOT serial_report MATCHES "\"requested_jobs\":1,\"effective_jobs\":1")
  message(FATAL_ERROR "serial report does not record one effective worker")
endif()

foreach(iteration RANGE 1 ${PARALLEL_REPEATS})
  if(iteration EQUAL 1)
    set(parallel_output "${OUTPUT_PARALLEL}")
    set(parallel_report_path "${REPORT_PARALLEL}")
  else()
    set(parallel_output "${OUTPUT_PARALLEL}.repeat${iteration}")
    set(parallel_report_path "${REPORT_PARALLEL}.repeat${iteration}")
  endif()
  file(REMOVE "${parallel_output}" "${parallel_report_path}")
  execute_process(
    COMMAND "${STEP_G}" -j ${PARALLEL_JOBS} -O "${parallel_output}"
      --report "${parallel_report_path}" "${INPUT}"
    RESULT_VARIABLE parallel_result
    OUTPUT_VARIABLE parallel_stdout
    ERROR_VARIABLE parallel_stderr
  )
  if(NOT parallel_result EQUAL 0)
    message(FATAL_ERROR
      "parallel determinism import ${iteration} failed (${parallel_result})\n"
      "${parallel_stdout}${parallel_stderr}")
  endif()

  file(SHA256 "${parallel_output}" parallel_hash)
  if(NOT serial_hash STREQUAL parallel_hash)
    message(FATAL_ERROR
      "-j 1 and -j ${PARALLEL_JOBS} output ${iteration} differ: "
      "${serial_hash} != ${parallel_hash}")
  endif()

  file(READ "${parallel_report_path}" parallel_report)
  if(NOT parallel_report MATCHES
      "\"requested_jobs\":${PARALLEL_JOBS},\"effective_jobs\":${PARALLEL_JOBS}")
    message(FATAL_ERROR
      "parallel report ${iteration} does not record ${PARALLEL_JOBS} effective workers")
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
endforeach()
