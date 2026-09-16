if(NOT DEFINED RTAREA OR NOT DEFINED DB OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "RTAREA, DB, and TEST_DIR are required")
endif()

if(NOT EXISTS "${RTAREA}")
  message(FATAL_ERROR "rtarea executable not found: ${RTAREA}")
endif()
if(NOT EXISTS "${DB}")
  message(FATAL_ERROR "rtarea test database not found: ${DB}")
endif()

set(image_side 64)
set(parallel_cpus 4)

function(run_rtarea output_var cpus)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env "LIBRT_EXP_MODE=0" "LIBRT_RAND_MODE=0"
      "${RTAREA}" -P "${cpus}" -s "${image_side}" ${ARGN} "${DB}" all.g
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
    RESULT_VARIABLE run_result
  )
  if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "rtarea ${ARGN} failed:\n${run_stdout}${run_stderr}")
  endif()
  set(${output_var} "${run_stdout}${run_stderr}" PARENT_SCOPE)
endfunction()

function(stable_report output_var run_log)
  string(
    CONCAT report_pattern
    "(Region|Assembly) [^\r\n]*"
    "|Cumulative Presented Areas[^\r\n]*"
    "|Total Exposed Area[^\r\n]*"
    "|Number of (Presented|Exposed) (Regions|Assemblies):[^\r\n]*"
  )
  string(
    REGEX MATCHALL
    "${report_pattern}"
    report_lines
    "${run_log}"
  )
  if(report_lines STREQUAL "")
    message(FATAL_ERROR "No rtarea report found in:\n${run_log}")
  endif()
  set(${output_var} "${report_lines}" PARENT_SCOPE)
endfunction()

function(require_same_report expected actual description)
  stable_report(expected_report "${expected}")
  stable_report(actual_report "${actual}")
  if(NOT expected_report STREQUAL actual_report)
    message(
      FATAL_ERROR
      "rtarea ${description} report differs:\n"
      "expected: ${expected_report}\n"
      "actual:   ${actual_report}"
    )
  endif()
endfunction()

run_rtarea(normal_serial 1)
run_rtarea(normal_parallel "${parallel_cpus}")
run_rtarea(incremental_serial 1 -i)
run_rtarea(incremental_parallel "${parallel_cpus}" -i)

require_same_report("${normal_serial}" "${normal_parallel}" "parallel")
require_same_report("${normal_serial}" "${incremental_serial}" "incremental serial")
require_same_report("${normal_serial}" "${incremental_parallel}" "incremental parallel")

run_rtarea(center_serial 1 -c "set compute_centers=1")
run_rtarea(center_incremental "${parallel_cpus}" -i -c "set compute_centers=1")
require_same_report("${center_serial}" "${center_incremental}" "incremental centers")

run_rtarea(jitter_serial 1 -J 1)
run_rtarea(jitter_incremental "${parallel_cpus}" -i -J 1)
require_same_report("${jitter_serial}" "${jitter_incremental}" "incremental jitter")

foreach(run_number RANGE 1 3)
  run_rtarea(jitter_parallel "${parallel_cpus}" -J 1)
  require_same_report("${jitter_serial}" "${jitter_parallel}" "parallel jitter run ${run_number}")
endforeach()

run_rtarea(hypersample_serial 1 -H 4)
run_rtarea(hypersample_incremental "${parallel_cpus}" -i -H 4)
require_same_report("${hypersample_serial}" "${hypersample_incremental}" "incremental hypersampling")

set(frame_script "${TEST_DIR}/rtarea-multiframe.rt")
# Fixed view intersects all regions in the moss test database.
file(
  WRITE "${frame_script}"
  "viewsize 3.000000000000000e+02;\n"
  "orientation 2.4809e-01 4.7650e-01 7.4809e-01 3.8943e-01;\n"
  "eye_pt 4.0000e+02 2.0000e+02 2.0000e+02;\n"
  "start 0; end;\n"
  "start 1; end;\n"
)
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env "LIBRT_EXP_MODE=0" "LIBRT_RAND_MODE=0"
    "${RTAREA}" -M -P "${parallel_cpus}" -s 16 "${DB}" all.g
  INPUT_FILE "${frame_script}"
  OUTPUT_VARIABLE frame_stdout
  ERROR_VARIABLE frame_stderr
  RESULT_VARIABLE frame_result
)
file(REMOVE "${frame_script}")
if(NOT frame_result EQUAL 0)
  message(FATAL_ERROR "rtarea multi-frame render failed:\n${frame_stdout}${frame_stderr}")
endif()
set(frame_log "${frame_stdout}${frame_stderr}")
string(REGEX MATCHALL "Cumulative Presented Areas[^\r\n]*|Total Exposed Area[^\r\n]*" frame_totals "${frame_log}")
list(LENGTH frame_totals frame_total_count)
if(NOT frame_total_count EQUAL 4)
  message(FATAL_ERROR "Expected two rtarea summaries, found ${frame_total_count} total lines")
endif()
list(GET frame_totals 0 frame_0_presented)
list(GET frame_totals 1 frame_0_exposed)
list(GET frame_totals 2 frame_1_presented)
list(GET frame_totals 3 frame_1_exposed)
if(NOT frame_0_presented STREQUAL frame_1_presented OR NOT frame_0_exposed STREQUAL frame_1_exposed)
  message(FATAL_ERROR "rtarea totals changed between identical frames:\n${frame_totals}")
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env "LIBRT_EXP_MODE=1" "LIBRT_RAND_MODE=0"
    "${RTAREA}" -P "${parallel_cpus}" -s "${image_side}" "${DB}" all.g
  OUTPUT_VARIABLE full_incremental_stdout
  ERROR_VARIABLE full_incremental_stderr
  RESULT_VARIABLE full_incremental_result
)
if(NOT full_incremental_result EQUAL 0)
  message(
    FATAL_ERROR
    "rtarea fully incremental render failed:\n${full_incremental_stdout}${full_incremental_stderr}"
  )
endif()
set(full_incremental_log "${full_incremental_stdout}${full_incremental_stderr}")
foreach(area_label IN ITEMS "Cumulative Presented Areas" "Total Exposed Area")
  string(REGEX MATCH "${area_label}[^=]*=[ ]*([0-9.]+) square" normal_match "${normal_serial}")
  set(normal_area "${CMAKE_MATCH_1}")
  string(REGEX MATCH "${area_label}[^=]*=[ ]*([0-9.]+) square" full_match "${full_incremental_log}")
  set(full_incremental_area "${CMAKE_MATCH_1}")
  if(normal_area STREQUAL "" OR full_incremental_area STREQUAL "")
    message(FATAL_ERROR "Unable to parse ${area_label} from rtarea output")
  endif()
  if(NOT normal_area STREQUAL full_incremental_area)
    message(
      FATAL_ERROR
      "Fully incremental ${area_label} differs: ${normal_area} != ${full_incremental_area}"
    )
  endif()
endforeach()
