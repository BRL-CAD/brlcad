# BRL-CAD
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
# SPDX-License-Identifier: BSD-3-Clause

file(MAKE_DIRECTORY "${TEST_DIR}/cache")
set(ENV{LIBRT_CACHE} "${TEST_DIR}/cache")
set(ENV{BU_DIR_CACHE} "${TEST_DIR}/cache")

function(run_checked step)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
    OUTPUT_FILE "${TEST_DIR}/${step}.log" ERROR_FILE "${TEST_DIR}/${step}.log"
    WORKING_DIRECTORY "${TEST_DIR}" TIMEOUT 120)
  if(NOT "${status}" STREQUAL "0")
    file(READ "${TEST_DIR}/${step}.log" output)
    message(FATAL_ERROR "${step} failed (${status}):\n${output}")
  endif()
endfunction()

function(check_complete_report path)
  file(READ "${path}" report)
  string(JSON success GET "${report}" success)
  if(NOT success)
    message(FATAL_ERROR "Import reported failure: ${path}")
  endif()
  foreach(statistic omitted unresolved_members unresolved_output_references)
    string(JSON count GET "${report}" statistics ${statistic})
    if(NOT count EQUAL 0)
      message(FATAL_ERROR "${path} reported ${statistic}=${count}")
    endif()
  endforeach()
endfunction()
