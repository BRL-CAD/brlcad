#                 B R E P _ R A Y T R A C E _ M A T R I X . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# Reproducible CPU-count/view matrix around brep_raytrace.cmake.  Each leaf
# run is an independently archived alternating benchmark and requires exact
# routed/branch-legacy output.  Performance gates are recorded rather than
# stopping the matrix, so a failed configuration remains available to inspect.

cmake_minimum_required(VERSION 3.20)

foreach(required_variable BRANCH_BUILD BASELINE_BUILD)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR
      "${required_variable} must name a configured build directory")
  endif()
endforeach()

if(NOT DEFINED SOURCE_DIR OR "${SOURCE_DIR}" STREQUAL "")
  cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH SOURCE_DIR)
endif()
if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
  set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/brep-raytrace-matrix")
endif()
if(NOT DEFINED PROCESSOR_COUNTS OR "${PROCESSOR_COUNTS}" STREQUAL "")
  set(PROCESSOR_COUNTS 1 2 all)
endif()
if(NOT DEFINED VIEW_AZIMUTHS OR "${VIEW_AZIMUTHS}" STREQUAL "")
  set(VIEW_AZIMUTHS 35 125 215)
endif()
if(NOT DEFINED VIEW_ELEVATIONS OR "${VIEW_ELEVATIONS}" STREQUAL "")
  set(VIEW_ELEVATIONS 25 -20 15)
endif()
if(NOT DEFINED CASES OR "${CASES}" STREQUAL "")
  set(CASES multiface imported)
endif()
if(NOT DEFINED TRIALS)
  set(TRIALS 7)
endif()
if(NOT DEFINED WARMUPS)
  set(WARMUPS 1)
endif()
if(NOT DEFINED COBB_SIZE)
  set(COBB_SIZE 512)
endif()
if(NOT DEFINED COBB_GRID)
  set(COBB_GRID 0.004)
endif()
if(NOT DEFINED MULTIFACE_SIZE)
  set(MULTIFACE_SIZE 1024)
endif()
if(NOT DEFINED IMPORTED_SIZE)
  set(IMPORTED_SIZE 1024)
endif()
if(NOT DEFINED LIGHT_MODEL)
  set(LIGHT_MODEL "")
endif()
if(NOT DEFINED ALLOW_EXISTING_OUTPUT)
  set(ALLOW_EXISTING_OUTPUT OFF)
endif()

file(REAL_PATH "${BRANCH_BUILD}" BRANCH_BUILD EXPAND_TILDE)
file(REAL_PATH "${BASELINE_BUILD}" BASELINE_BUILD EXPAND_TILDE)
file(REAL_PATH "${SOURCE_DIR}" SOURCE_DIR EXPAND_TILDE)
cmake_path(ABSOLUTE_PATH OUTPUT_DIR NORMALIZE OUTPUT_VARIABLE OUTPUT_DIR)
set(leaf_driver "${SOURCE_DIR}/bench/brep_raytrace.cmake")
if(NOT EXISTS "${leaf_driver}")
  message(FATAL_ERROR "Cannot find leaf benchmark driver: ${leaf_driver}")
endif()

if(EXISTS "${OUTPUT_DIR}/matrix-manifest.txt" AND NOT ALLOW_EXISTING_OUTPUT)
  message(FATAL_ERROR
    "${OUTPUT_DIR} already contains a matrix archive; choose a new OUTPUT_DIR "
    "or set ALLOW_EXISTING_OUTPUT=ON explicitly"
  )
endif()

if(TRIALS LESS 1)
  message(FATAL_ERROR "TRIALS must be at least one")
endif()
math(EXPR trials_remainder "${TRIALS} % 2")
if(TRIALS GREATER 1 AND NOT trials_remainder EQUAL 1)
  message(FATAL_ERROR "TRIALS must be odd")
endif()
if(WARMUPS LESS 0)
  message(FATAL_ERROR "WARMUPS may not be negative")
endif()

list(LENGTH VIEW_AZIMUTHS azimuth_count)
list(LENGTH VIEW_ELEVATIONS elevation_count)
if(NOT azimuth_count EQUAL elevation_count OR azimuth_count EQUAL 0)
  message(FATAL_ERROR
    "VIEW_AZIMUTHS and VIEW_ELEVATIONS must be nonempty parallel lists")
endif()
foreach(value IN LISTS VIEW_AZIMUTHS VIEW_ELEVATIONS)
  if(NOT "${value}" MATCHES "^-?[0-9]+([.][0-9]+)?$")
    message(FATAL_ERROR "View value '${value}' is not numeric")
  endif()
endforeach()
if(NOT "${LIGHT_MODEL}" STREQUAL "" AND
   NOT "${LIGHT_MODEL}" MATCHES "^[0-9]+$")
  message(FATAL_ERROR "LIGHT_MODEL must be empty or a nonnegative integer")
endif()

cmake_host_system_information(RESULT logical_cores QUERY NUMBER_OF_LOGICAL_CORES)
set(resolved_processor_counts)
foreach(processors IN LISTS PROCESSOR_COUNTS)
  if(processors STREQUAL "all")
    set(processors "${logical_cores}")
  endif()
  if(NOT "${processors}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
      "PROCESSOR_COUNTS entries must be positive integers or 'all'")
  endif()
  list(APPEND resolved_processor_counts "${processors}")
endforeach()
list(REMOVE_DUPLICATES resolved_processor_counts)

foreach(case_name IN LISTS CASES)
  if(NOT case_name STREQUAL "cobb" AND
      NOT case_name STREQUAL "multiface" AND
      NOT case_name STREQUAL "imported")
    message(FATAL_ERROR "Unknown benchmark case '${case_name}'")
  endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
string(TIMESTAMP run_timestamp "%Y-%m-%dT%H:%M:%SZ" UTC)
file(SHA256 "${leaf_driver}" leaf_driver_sha256)
file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" matrix_driver_sha256)
set(matrix_manifest "${OUTPUT_DIR}/matrix-manifest.txt")
set(matrix_results "${OUTPUT_DIR}/matrix-results.csv")
set(matrix_summary "${OUTPUT_DIR}/matrix-summary.txt")
set(matrix_runs "${OUTPUT_DIR}/matrix-runs.csv")
file(WRITE "${matrix_manifest}"
  "BREP raytrace scaling/view matrix manifest\n"
  "timestamp_utc=${run_timestamp}\n"
  "host_logical_cores=${logical_cores}\n"
  "source_dir=${SOURCE_DIR}\n"
  "branch_build=${BRANCH_BUILD}\n"
  "baseline_build=${BASELINE_BUILD}\n"
  "matrix_driver=${CMAKE_CURRENT_LIST_FILE}\n"
  "matrix_driver_sha256=${matrix_driver_sha256}\n"
  "leaf_driver=${leaf_driver}\n"
  "leaf_driver_sha256=${leaf_driver_sha256}\n"
  "processor_counts=${resolved_processor_counts}\n"
  "view_azimuths=${VIEW_AZIMUTHS}\n"
  "view_elevations=${VIEW_ELEVATIONS}\n"
  "cases=${CASES}\n"
  "trials=${TRIALS}\n"
  "warmups=${WARMUPS}\n"
  "cobb_size=${COBB_SIZE}\n"
  "cobb_grid=${COBB_GRID}\n"
  "multiface_size=${MULTIFACE_SIZE}\n"
  "imported_size=${IMPORTED_SIZE}\n"
  "light_model=${LIGHT_MODEL}\n"
  "cache=disabled\n"
  "routed_legacy_exact_match=required\n"
)
file(WRITE "${matrix_results}"
  "configuration,processors,azimuth,elevation,case,implementation,trial,order,rays,intersections,hits,misses,shot_cpu_seconds,shot_elapsed_seconds,rays_per_cpu_second,rays_per_wall_second,result_sha256,log\n"
)
file(WRITE "${matrix_runs}"
  "configuration,case,processors,azimuth,elevation,cpu_main_gate,wall_main_gate,cpu_legacy_gate,wall_legacy_gate,manifest_sha256,results_sha256,summary_sha256\n"
)
file(WRITE "${matrix_summary}"
  "BREP raytrace scaling/view matrix\n"
  "Generated ${run_timestamp}; exact routed/legacy output is mandatory.\n\n"
)

set(extra_leaf_arguments)
if(DEFINED IMPORTED_DATABASE AND NOT "${IMPORTED_DATABASE}" STREQUAL "")
  list(APPEND extra_leaf_arguments "-DIMPORTED_DATABASE=${IMPORTED_DATABASE}")
endif()
if(DEFINED IMPORTED_OBJECT AND NOT "${IMPORTED_OBJECT}" STREQUAL "")
  list(APPEND extra_leaf_arguments "-DIMPORTED_OBJECT=${IMPORTED_OBJECT}")
endif()

set(configuration_count 0)
set(all_gate_count 0)
set(failed_gate_count 0)
math(EXPR last_view_index "${azimuth_count} - 1")
foreach(processors IN LISTS resolved_processor_counts)
  foreach(view_index RANGE 0 ${last_view_index})
    list(GET VIEW_AZIMUTHS ${view_index} azimuth)
    list(GET VIEW_ELEVATIONS ${view_index} elevation)
    string(REPLACE "-" "m" azimuth_label "${azimuth}")
    string(REPLACE "." "p" azimuth_label "${azimuth_label}")
    string(REPLACE "-" "m" elevation_label "${elevation}")
    string(REPLACE "." "p" elevation_label "${elevation_label}")
    foreach(case_name IN LISTS CASES)
      set(configuration
        "${case_name}-p${processors}-a${azimuth_label}-e${elevation_label}")
      set(leaf_output "${OUTPUT_DIR}/${configuration}")
      file(MAKE_DIRECTORY "${leaf_output}")
      message(STATUS "Matrix configuration ${configuration}")
      execute_process(
        COMMAND "${CMAKE_COMMAND}"
          "-DBRANCH_BUILD=${BRANCH_BUILD}"
          "-DBASELINE_BUILD=${BASELINE_BUILD}"
          "-DSOURCE_DIR=${SOURCE_DIR}"
          "-DOUTPUT_DIR=${leaf_output}"
          "-DCASES=${case_name}"
          "-DTRIALS=${TRIALS}"
          "-DWARMUPS=${WARMUPS}"
          "-DINCLUDE_LEGACY=ON"
          "-DINCLUDE_ROUTED=ON"
          "-DREQUIRE_ROUTED_LEGACY_MATCH=ON"
          "-DBRANCH_DIRECT_ROUTING=ON"
          "-DROUTED_PUBLICATION=ON"
          "-DROUTED_MISSING_ROOT_CERTIFICATE=ON"
          "-DPROCESSORS=${processors}"
          "-DAZIMUTH=${azimuth}"
          "-DELEVATION=${elevation}"
          "-DCOBB_SIZE=${COBB_SIZE}"
          "-DCOBB_GRID=${COBB_GRID}"
          "-DMULTIFACE_SIZE=${MULTIFACE_SIZE}"
          "-DIMPORTED_SIZE=${IMPORTED_SIZE}"
          "-DLIGHT_MODEL=${LIGHT_MODEL}"
          ${extra_leaf_arguments}
          -P "${leaf_driver}"
        RESULT_VARIABLE leaf_result
        OUTPUT_VARIABLE leaf_stdout
        ERROR_VARIABLE leaf_stderr
      )
      file(WRITE "${leaf_output}/matrix-driver.log"
        "${leaf_stdout}${leaf_stderr}")
      if(NOT leaf_result EQUAL 0)
        message(FATAL_ERROR
          "Matrix configuration ${configuration} failed (${leaf_result}); "
          "see ${leaf_output}/matrix-driver.log"
        )
      endif()

      set(leaf_results_file "${leaf_output}/results.csv")
      set(leaf_summary_file "${leaf_output}/summary.txt")
      file(STRINGS "${leaf_results_file}" leaf_result_lines)
      list(POP_FRONT leaf_result_lines)
      foreach(result_line IN LISTS leaf_result_lines)
        file(APPEND "${matrix_results}"
          "${configuration},${processors},${azimuth},${elevation},${result_line}\n")
      endforeach()

      file(READ "${leaf_summary_file}" leaf_summary)
      file(APPEND "${matrix_summary}"
        "${configuration}\n"
        "processors=${processors}, azimuth=${azimuth}, elevation=${elevation}\n"
        "${leaf_summary}\n"
      )

      set(cpu_main_gate FAIL)
      set(wall_main_gate FAIL)
      set(cpu_legacy_gate FAIL)
      set(wall_legacy_gate FAIL)
      string(FIND "${leaf_summary}"
        "routed CPU gate (>=100% baseline): PASS" gate_position)
      if(NOT gate_position EQUAL -1)
        set(cpu_main_gate PASS)
      endif()
      string(FIND "${leaf_summary}"
        "routed wall gate (>=100% baseline): PASS" gate_position)
      if(NOT gate_position EQUAL -1)
        set(wall_main_gate PASS)
      endif()
      string(FIND "${leaf_summary}"
        "routed CPU near-legacy gate (>=95%): PASS" gate_position)
      if(NOT gate_position EQUAL -1)
        set(cpu_legacy_gate PASS)
      endif()
      string(FIND "${leaf_summary}"
        "routed wall near-legacy gate (>=95%): PASS" gate_position)
      if(NOT gate_position EQUAL -1)
        set(wall_legacy_gate PASS)
      endif()
      foreach(gate cpu_main_gate wall_main_gate cpu_legacy_gate wall_legacy_gate)
        math(EXPR all_gate_count "${all_gate_count} + 1")
        if("${${gate}}" STREQUAL "FAIL")
          math(EXPR failed_gate_count "${failed_gate_count} + 1")
        endif()
      endforeach()

      file(SHA256 "${leaf_output}/manifest.txt" leaf_manifest_sha256)
      file(SHA256 "${leaf_results_file}" leaf_results_sha256)
      file(SHA256 "${leaf_summary_file}" leaf_summary_sha256)
      file(APPEND "${matrix_runs}"
        "${configuration},${case_name},${processors},${azimuth},${elevation},${cpu_main_gate},${wall_main_gate},${cpu_legacy_gate},${wall_legacy_gate},${leaf_manifest_sha256},${leaf_results_sha256},${leaf_summary_sha256}\n"
      )
      math(EXPR configuration_count "${configuration_count} + 1")
    endforeach()
  endforeach()
endforeach()

file(SHA256 "${matrix_results}" matrix_results_sha256)
file(SHA256 "${matrix_runs}" matrix_runs_sha256)
file(APPEND "${matrix_manifest}"
  "configuration_count=${configuration_count}\n"
  "performance_gate_count=${all_gate_count}\n"
  "performance_gate_failures=${failed_gate_count}\n"
  "matrix_results_sha256=${matrix_results_sha256}\n"
  "matrix_runs_sha256=${matrix_runs_sha256}\n"
)
file(APPEND "${matrix_summary}"
  "Matrix totals\n"
  "  configurations: ${configuration_count}\n"
  "  performance gates: ${all_gate_count}\n"
  "  performance gate failures: ${failed_gate_count}\n"
  "  exact routed/legacy contract: PASS for every completed configuration\n"
)

if(failed_gate_count GREATER 0)
  message(WARNING
    "${failed_gate_count}/${all_gate_count} performance gates failed; "
    "exact-output gates passed.  See ${matrix_summary}"
  )
endif()
message(STATUS "Benchmark matrix complete: ${matrix_summary}")
