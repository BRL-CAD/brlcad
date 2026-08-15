#              R U N _ C M D _ A N A L Y S I S . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.

# The command-analysis fixture deliberately creates and renames database
# objects.  Always run it on a build-tree copy so repeated CTest invocations
# do not alter the source fixture or each other's input.
foreach(required TEST_EXECUTABLE SOURCE_DB WORK_DB)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "run_cmd_analysis.cmake requires -D${required}=...")
  endif()
endforeach()

get_filename_component(work_dir "${WORK_DB}" DIRECTORY)
file(MAKE_DIRECTORY "${work_dir}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy "${SOURCE_DB}" "${WORK_DB}"
  RESULT_VARIABLE copy_result
)
if(NOT copy_result EQUAL 0)
  message(FATAL_ERROR "Unable to copy command-analysis database fixture")
endif()

set(command "${TEST_EXECUTABLE}" "${WORK_DB}")
if(DEFINED EXTRA_DB AND NOT "${EXTRA_DB}" STREQUAL "")
  # The optional fixture is writable too: the BREP completion checks create a
  # temporary database object.  Keep it isolated for the same reason as the
  # primary fixture so this test remains repeatable and parallel-safe.
  get_filename_component(extra_name "${EXTRA_DB}" NAME)
  set(extra_work_db "${work_dir}/extra_${extra_name}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy "${EXTRA_DB}" "${extra_work_db}"
    RESULT_VARIABLE extra_copy_result
  )
  if(NOT extra_copy_result EQUAL 0)
    message(FATAL_ERROR "Unable to copy optional command-analysis database fixture")
  endif()
  list(APPEND command "${extra_work_db}")
endif()

execute_process(COMMAND ${command} RESULT_VARIABLE test_result)
if(NOT test_result EQUAL 0)
  message(FATAL_ERROR "ged_test_cmd_analysis failed with status ${test_result}")
endif()
