#                    P G O _ B E N C H M A R K . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
#    products derived from this software without specific prior written
#    permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.22)

# TRAIN runs the standard benchmark and additional geometry, then verifies
# that compiler profile counters were produced.  COMPARE runs the standard
# benchmark from baseline and profile-use build trees and requires its
# equal-weight mean speedup to exceed PGO_MIN_SPEEDUP.  The training model
# list accepts build-relative or absolute paths, so external model sets can be
# supplied without changing this script.
#
# Required inputs:
#   TRAIN:   PGO_BUILD_DIR, PGO_RESULTS_DIR, PGO_PROFILE_DIR
#   COMPARE: PGO_BASELINE_BUILD_DIR, PGO_OPTIMIZED_BUILD_DIR,
#            PGO_RESULTS_DIR

set(_pgo_valid_modes TRAIN COMPARE)
if(NOT DEFINED PGO_MODE)
  message(FATAL_ERROR "PGO_MODE must be TRAIN or COMPARE")
endif()

string(TOUPPER "${PGO_MODE}" PGO_MODE)
if(NOT PGO_MODE IN_LIST _pgo_valid_modes)
  message(FATAL_ERROR "PGO_MODE must be TRAIN or COMPARE, not '${PGO_MODE}'")
endif()

set(_pgo_default_models moss world star bldg391 m35 sphflake)
set(
  _pgo_default_training_models
  share/db/havoc.g
  share/db/faa/Generic_Twin.g
  share/db/nist/NIST_MBE_PMI_2.g
)

if(NOT DEFINED PGO_MODELS)
  set(PGO_MODELS ${_pgo_default_models})
endif()
if(NOT DEFINED PGO_TRAINING_MODELS)
  set(PGO_TRAINING_MODELS ${_pgo_default_training_models})
endif()
if(NOT DEFINED PGO_TIMEFRAME)
  set(PGO_TIMEFRAME 8)
endif()
if(NOT DEFINED PGO_MAXTIME)
  set(PGO_MAXTIME 60)
endif()
if(NOT DEFINED PGO_AVERAGE)
  set(PGO_AVERAGE 3)
endif()
if(NOT DEFINED PGO_DEVIATION)
  set(PGO_DEVIATION 3)
endif()
if(NOT DEFINED PGO_TRAINING_SIZE)
  set(PGO_TRAINING_SIZE 512)
endif()
if(NOT DEFINED PGO_TRAINING_AZIMUTH)
  set(PGO_TRAINING_AZIMUTH 35)
endif()
if(NOT DEFINED PGO_TRAINING_ELEVATION)
  set(PGO_TRAINING_ELEVATION 25)
endif()
if(NOT DEFINED PGO_MIN_SPEEDUP)
  set(PGO_MIN_SPEEDUP 1.0)
endif()

function(_pgo_require variable_name)
  if(NOT DEFINED "${variable_name}" OR "${${variable_name}}" STREQUAL "")
    message(FATAL_ERROR "${variable_name} is required for PGO_MODE=${PGO_MODE}")
  endif()
endfunction()

function(_pgo_absolute_path input_path output_variable)
  get_filename_component(_pgo_path "${input_path}" ABSOLUTE)
  file(TO_CMAKE_PATH "${_pgo_path}" _pgo_path)
  set("${output_variable}" "${_pgo_path}" PARENT_SCOPE)
endfunction()

function(_pgo_cache_value build_directory variable_name output_variable)
  set(_pgo_cache_file "${build_directory}/CMakeCache.txt")
  if(NOT EXISTS "${_pgo_cache_file}")
    message(FATAL_ERROR "CMake cache does not exist: ${_pgo_cache_file}")
  endif()
  file(
    STRINGS
    "${_pgo_cache_file}"
    _pgo_cache_entry
    LIMIT_COUNT 1
    REGEX "^${variable_name}(:[^=]*)?="
  )
  if(_pgo_cache_entry)
    string(REGEX REPLACE "^[^=]*=" "" _pgo_value "${_pgo_cache_entry}")
  else()
    set(_pgo_value "")
  endif()
  set("${output_variable}" "${_pgo_value}" PARENT_SCOPE)
endfunction()

function(_pgo_validate_build build_directory expected_pgo expected_msvc_phase)
  _pgo_cache_value("${build_directory}" BRLCAD_PGO _pgo_enabled)
  if(expected_pgo AND NOT _pgo_enabled)
    message(FATAL_ERROR "BRLCAD_PGO is not enabled in ${build_directory}")
  elseif(NOT expected_pgo AND _pgo_enabled)
    message(FATAL_ERROR "The baseline build unexpectedly enables BRLCAD_PGO: ${build_directory}")
  endif()

  _pgo_cache_value("${build_directory}" BRLCAD_MSVC_PGO_PHASE _pgo_msvc_phase)
  if(_pgo_msvc_phase AND NOT _pgo_msvc_phase STREQUAL expected_msvc_phase)
    message(
      FATAL_ERROR
      "Expected MSVC PGO phase ${expected_msvc_phase} in ${build_directory}, found ${_pgo_msvc_phase}"
    )
  endif()
endfunction()

function(_pgo_find_build_file build_directory file_name output_variable)
  set(_pgo_candidates "${build_directory}/bin/${file_name}")
  if(DEFINED PGO_CONFIG AND NOT "${PGO_CONFIG}" STREQUAL "")
    list(
      APPEND
      _pgo_candidates
      "${build_directory}/bin/${PGO_CONFIG}/${file_name}"
      "${build_directory}/${PGO_CONFIG}/bin/${file_name}"
    )
  endif()
  if(CMAKE_HOST_WIN32)
    set(_pgo_executable_candidates "")
    foreach(_pgo_candidate IN LISTS _pgo_candidates)
      list(APPEND _pgo_executable_candidates "${_pgo_candidate}.exe")
    endforeach()
    list(APPEND _pgo_candidates ${_pgo_executable_candidates})
  endif()

  foreach(_pgo_candidate IN LISTS _pgo_candidates)
    if(EXISTS "${_pgo_candidate}")
      set("${output_variable}" "${_pgo_candidate}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  message(FATAL_ERROR "Could not find ${file_name} under ${build_directory}/bin")
endfunction()

function(_pgo_find_shell build_directory output_variable)
  if(DEFINED PGO_SHELL AND NOT "${PGO_SHELL}" STREQUAL "")
    set(_pgo_shell "${PGO_SHELL}")
  elseif(EXISTS "${build_directory}/CMakeCache.txt")
    file(
      STRINGS
      "${build_directory}/CMakeCache.txt"
      _pgo_shell_cache_entry
      LIMIT_COUNT 1
      REGEX "^SH_EXEC(:[^=]*)?="
    )
    if(_pgo_shell_cache_entry)
      string(REGEX REPLACE "^[^=]*=" "" _pgo_shell "${_pgo_shell_cache_entry}")
    endif()
  endif()

  if(NOT DEFINED _pgo_shell OR "${_pgo_shell}" STREQUAL "" OR NOT EXISTS "${_pgo_shell}")
    unset(_pgo_shell)
    unset(_pgo_shell CACHE)
    find_program(_pgo_shell NAMES sh bash)
  endif()
  if(NOT _pgo_shell)
    message(FATAL_ERROR "The BRL-CAD benchmark requires a POSIX shell; set PGO_SHELL to its path")
  endif()
  set("${output_variable}" "${_pgo_shell}" PARENT_SCOPE)
endfunction()

function(_pgo_run_benchmark build_directory result_directory)
  _pgo_find_build_file("${build_directory}" benchmark _pgo_benchmark)
  _pgo_find_build_file("${build_directory}" rt _pgo_rt)
  _pgo_find_build_file("${build_directory}" pixcmp _pgo_pixcmp)
  _pgo_find_shell("${build_directory}" _pgo_shell)
  file(MAKE_DIRECTORY "${result_directory}")

  message(STATUS "Running the BRL-CAD benchmark from ${build_directory}")
  execute_process(
    COMMAND
      "${_pgo_shell}" "${_pgo_benchmark}" run
      "TIMEFRAME=${PGO_TIMEFRAME}"
      "MAXTIME=${PGO_MAXTIME}"
      "AVERAGE=${PGO_AVERAGE}"
      "DEVIATION=${PGO_DEVIATION}"
      "RT=${_pgo_rt}"
      "CMP=${_pgo_pixcmp}"
      -P1
    WORKING_DIRECTORY "${result_directory}"
    RESULT_VARIABLE _pgo_benchmark_result
  )
  if(NOT "${_pgo_benchmark_result}" STREQUAL "0")
    message(FATAL_ERROR "The benchmark from ${build_directory} failed with status ${_pgo_benchmark_result}")
  endif()
endfunction()

function(_pgo_extract_rate log_file output_text output_integer)
  if(NOT EXISTS "${log_file}")
    message(FATAL_ERROR "Benchmark log does not exist: ${log_file}")
  endif()
  file(READ "${log_file}" _pgo_log)
  string(REGEX MATCHALL "[0-9]+(\\.[0-9]+)? rays/sec \\(RTFM\\)" _pgo_rate_lines "${_pgo_log}")
  if(NOT _pgo_rate_lines)
    message(FATAL_ERROR "No RTFM result was found in ${log_file}")
  endif()
  list(GET _pgo_rate_lines -1 _pgo_rate_line)
  string(REGEX REPLACE " rays/sec.*$" "" _pgo_rate "${_pgo_rate_line}")

  if(NOT _pgo_rate MATCHES "^([0-9]+)(\\.([0-9]+))?$")
    message(FATAL_ERROR "Invalid RTFM result '${_pgo_rate}' in ${log_file}")
  endif()
  set(_pgo_whole "${CMAKE_MATCH_1}")
  set(_pgo_fraction "${CMAKE_MATCH_3}00")
  string(SUBSTRING "${_pgo_fraction}" 0 2 _pgo_fraction)
  string(REGEX REPLACE "^0+" "" _pgo_whole "${_pgo_whole}")
  string(REGEX REPLACE "^0+" "" _pgo_fraction "${_pgo_fraction}")
  if("${_pgo_whole}" STREQUAL "")
    set(_pgo_whole 0)
  endif()
  if("${_pgo_fraction}" STREQUAL "")
    set(_pgo_fraction 0)
  endif()
  math(EXPR _pgo_rate_integer "${_pgo_whole} * 100 + ${_pgo_fraction}")
  if(_pgo_rate_integer LESS_EQUAL 0)
    message(FATAL_ERROR "Non-positive RTFM result '${_pgo_rate}' in ${log_file}")
  endif()

  set("${output_text}" "${_pgo_rate}" PARENT_SCOPE)
  set("${output_integer}" "${_pgo_rate_integer}" PARENT_SCOPE)
endfunction()

function(_pgo_decimal_to_thousandths value output_variable)
  if(NOT "${value}" MATCHES "^([0-9]+)(\\.([0-9]+))?$")
    message(FATAL_ERROR "Expected a non-negative decimal number, not '${value}'")
  endif()
  set(_pgo_whole "${CMAKE_MATCH_1}")
  set(_pgo_fraction "${CMAKE_MATCH_3}000")
  string(SUBSTRING "${_pgo_fraction}" 0 3 _pgo_fraction)
  string(REGEX REPLACE "^0+" "" _pgo_whole "${_pgo_whole}")
  string(REGEX REPLACE "^0+" "" _pgo_fraction "${_pgo_fraction}")
  if("${_pgo_whole}" STREQUAL "")
    set(_pgo_whole 0)
  endif()
  if("${_pgo_fraction}" STREQUAL "")
    set(_pgo_fraction 0)
  endif()
  math(EXPR _pgo_value "${_pgo_whole} * 1000 + ${_pgo_fraction}")
  set("${output_variable}" "${_pgo_value}" PARENT_SCOPE)
endfunction()

function(_pgo_format_thousandths value output_variable)
  math(EXPR _pgo_whole "${value} / 1000")
  math(EXPR _pgo_fraction "${value} % 1000")
  set(_pgo_fraction "00${_pgo_fraction}")
  string(LENGTH "${_pgo_fraction}" _pgo_fraction_length)
  math(EXPR _pgo_fraction_offset "${_pgo_fraction_length} - 3")
  string(SUBSTRING "${_pgo_fraction}" ${_pgo_fraction_offset} 3 _pgo_fraction)
  set("${output_variable}" "${_pgo_whole}.${_pgo_fraction}" PARENT_SCOPE)
endfunction()

if(PGO_MODE STREQUAL "TRAIN")
  _pgo_require(PGO_BUILD_DIR)
  _pgo_require(PGO_RESULTS_DIR)
  _pgo_require(PGO_PROFILE_DIR)
  _pgo_absolute_path("${PGO_BUILD_DIR}" PGO_BUILD_DIR)
  _pgo_absolute_path("${PGO_RESULTS_DIR}" PGO_RESULTS_DIR)
  _pgo_absolute_path("${PGO_PROFILE_DIR}" PGO_PROFILE_DIR)
  if(NOT IS_DIRECTORY "${PGO_BUILD_DIR}")
    message(FATAL_ERROR "PGO_BUILD_DIR does not exist: ${PGO_BUILD_DIR}")
  endif()
  _pgo_validate_build("${PGO_BUILD_DIR}" TRUE GENERATE)

  # MSVC's profile runtime uses this location for each image's PGC files.
  # GCC ignores it and writes its GCDA files to the compiled-in PGO path.
  file(MAKE_DIRECTORY "${PGO_PROFILE_DIR}")
  set(ENV{VCPROFILE_PATH} "${PGO_PROFILE_DIR}")
  _pgo_run_benchmark("${PGO_BUILD_DIR}" "${PGO_RESULTS_DIR}/benchmark")

  _pgo_find_build_file("${PGO_BUILD_DIR}" rt _pgo_rt)
  if(CMAKE_HOST_WIN32)
    set(_pgo_null_device NUL)
  else()
    set(_pgo_null_device /dev/null)
  endif()

  set(_pgo_training_index 0)
  foreach(_pgo_model IN LISTS PGO_TRAINING_MODELS)
    math(EXPR _pgo_training_index "${_pgo_training_index} + 1")
    if(IS_ABSOLUTE "${_pgo_model}")
      set(_pgo_model_path "${_pgo_model}")
    else()
      set(_pgo_model_path "${PGO_BUILD_DIR}/${_pgo_model}")
    endif()
    if(NOT EXISTS "${_pgo_model_path}")
      message(FATAL_ERROR "Training model does not exist: ${_pgo_model_path}")
    endif()

    get_filename_component(_pgo_log_name "${_pgo_model_path}" NAME_WE)
    string(REGEX REPLACE "[^A-Za-z0-9_.-]" "_" _pgo_log_name "${_pgo_log_name}")
    set(_pgo_log_file "${PGO_RESULTS_DIR}/${_pgo_training_index}-${_pgo_log_name}.log")
    message(STATUS "Training with ${_pgo_model_path}")
    execute_process(
      COMMAND
        "${_pgo_rt}" -B -P1 "-s${PGO_TRAINING_SIZE}"
        "-a${PGO_TRAINING_AZIMUTH}" "-e${PGO_TRAINING_ELEVATION}"
        -o "${_pgo_null_device}" "${_pgo_model_path}"
      RESULT_VARIABLE _pgo_rt_result
      OUTPUT_VARIABLE _pgo_rt_output
      ERROR_VARIABLE _pgo_rt_error
    )
    file(WRITE "${_pgo_log_file}" "${_pgo_rt_output}${_pgo_rt_error}")
    if(NOT "${_pgo_rt_result}" STREQUAL "0")
      message(FATAL_ERROR "Training ${_pgo_model} failed with status ${_pgo_rt_result}; see ${_pgo_log_file}")
    endif()
    string(REGEX MATCH "[^\r\n]*rays/sec \\(RTFM\\)" _pgo_rtfm_line "${_pgo_rt_output}${_pgo_rt_error}")
    if(NOT _pgo_rtfm_line)
      message(FATAL_ERROR "Training ${_pgo_model} did not complete a raytrace; see ${_pgo_log_file}")
    endif()
    message(STATUS "${_pgo_model}: ${_pgo_rtfm_line}")
  endforeach()

  file(GLOB_RECURSE _pgo_gcda_files LIST_DIRECTORIES FALSE "${PGO_PROFILE_DIR}/*.gcda")
  file(GLOB_RECURSE _pgo_pgc_files LIST_DIRECTORIES FALSE "${PGO_PROFILE_DIR}/*.pgc")
  list(LENGTH _pgo_gcda_files _pgo_gcda_count)
  list(LENGTH _pgo_pgc_files _pgo_pgc_count)
  math(EXPR _pgo_profile_count "${_pgo_gcda_count} + ${_pgo_pgc_count}")
  if(_pgo_profile_count EQUAL 0)
    message(FATAL_ERROR "Training did not generate GCC GCDA or MSVC PGC data under ${PGO_PROFILE_DIR}")
  endif()
  message(STATUS "Training generated ${_pgo_profile_count} profile counter files under ${PGO_PROFILE_DIR}")
else()
  _pgo_require(PGO_BASELINE_BUILD_DIR)
  _pgo_require(PGO_OPTIMIZED_BUILD_DIR)
  _pgo_require(PGO_RESULTS_DIR)
  _pgo_absolute_path("${PGO_BASELINE_BUILD_DIR}" PGO_BASELINE_BUILD_DIR)
  _pgo_absolute_path("${PGO_OPTIMIZED_BUILD_DIR}" PGO_OPTIMIZED_BUILD_DIR)
  _pgo_absolute_path("${PGO_RESULTS_DIR}" PGO_RESULTS_DIR)
  _pgo_validate_build("${PGO_BASELINE_BUILD_DIR}" FALSE "")
  _pgo_validate_build("${PGO_OPTIMIZED_BUILD_DIR}" TRUE USE)

  _pgo_run_benchmark("${PGO_BASELINE_BUILD_DIR}" "${PGO_RESULTS_DIR}/baseline")
  _pgo_run_benchmark("${PGO_OPTIMIZED_BUILD_DIR}" "${PGO_RESULTS_DIR}/optimized")

  set(_pgo_report "### Profile-guided optimization results\n\n")
  string(APPEND _pgo_report "| Model | Baseline rays/sec | PGO rays/sec | Speedup |\n")
  string(APPEND _pgo_report "| --- | ---: | ---: | ---: |\n")
  set(_pgo_speedup_sum 0)
  set(_pgo_model_count 0)
  set(_pgo_tsv "model\tbaseline_rays_per_second\tpgo_rays_per_second\tspeedup\n")

  foreach(_pgo_model IN LISTS PGO_MODELS)
    _pgo_extract_rate("${PGO_RESULTS_DIR}/baseline/${_pgo_model}.log" _pgo_baseline_rate _pgo_baseline_integer)
    _pgo_extract_rate("${PGO_RESULTS_DIR}/optimized/${_pgo_model}.log" _pgo_optimized_rate _pgo_optimized_integer)
    math(
      EXPR
      _pgo_speedup
      "(${_pgo_optimized_integer} * 1000 + ${_pgo_baseline_integer} / 2) / ${_pgo_baseline_integer}"
    )
    _pgo_format_thousandths("${_pgo_speedup}" _pgo_speedup_text)
    math(EXPR _pgo_speedup_sum "${_pgo_speedup_sum} + ${_pgo_speedup}")
    math(EXPR _pgo_model_count "${_pgo_model_count} + 1")
    string(
      APPEND
      _pgo_report
      "| `${_pgo_model}` | ${_pgo_baseline_rate} | ${_pgo_optimized_rate} | ${_pgo_speedup_text}x |\n"
    )
    string(
      APPEND
      _pgo_tsv
      "${_pgo_model}\t${_pgo_baseline_rate}\t${_pgo_optimized_rate}\t${_pgo_speedup_text}\n"
    )
  endforeach()

  if(_pgo_model_count EQUAL 0)
    message(FATAL_ERROR "PGO_MODELS must name at least one benchmark model")
  endif()
  math(EXPR _pgo_mean_speedup "(${_pgo_speedup_sum} + ${_pgo_model_count} / 2) / ${_pgo_model_count}")
  _pgo_format_thousandths("${_pgo_mean_speedup}" _pgo_mean_speedup_text)
  string(APPEND _pgo_report "\nArithmetic mean speedup: ${_pgo_mean_speedup_text}x\n")
  file(MAKE_DIRECTORY "${PGO_RESULTS_DIR}")
  file(WRITE "${PGO_RESULTS_DIR}/results.tsv" "${_pgo_tsv}")

  if(DEFINED PGO_SUMMARY_FILE AND NOT "${PGO_SUMMARY_FILE}" STREQUAL "")
    _pgo_absolute_path("${PGO_SUMMARY_FILE}" _pgo_summary_file)
  elseif(NOT "$ENV{GITHUB_STEP_SUMMARY}" STREQUAL "")
    set(_pgo_summary_file "$ENV{GITHUB_STEP_SUMMARY}")
  endif()
  if(DEFINED _pgo_summary_file)
    file(APPEND "${_pgo_summary_file}" "${_pgo_report}")
  endif()
  message("${_pgo_report}")

  _pgo_decimal_to_thousandths("${PGO_MIN_SPEEDUP}" _pgo_min_speedup)
  if(NOT _pgo_mean_speedup GREATER _pgo_min_speedup)
    message(
      FATAL_ERROR
      "PGO raytracing speedup ${_pgo_mean_speedup_text}x did not exceed ${PGO_MIN_SPEEDUP}x"
    )
  endif()
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
