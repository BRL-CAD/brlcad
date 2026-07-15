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
  list(APPEND command "${EXTRA_DB}")
endif()

execute_process(COMMAND ${command} RESULT_VARIABLE test_result)
if(NOT test_result EQUAL 0)
  message(FATAL_ERROR "ged_test_cmd_analysis failed with status ${test_result}")
endif()
