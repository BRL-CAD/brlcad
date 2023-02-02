#      B R L C A D _ T E S T _ W R A P P E R S . C M A K E
#
# BRL-CAD
#
# Copyright (c) 2020-2023 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

# "make unit"  runs all the unit tests.
# To build the required targets for testing in the style of GNU Autotools "make
# check") we define "unit" and "check" targets per
# http://www.cmake.org/Wiki/CMakeEmulateMakeCheck and have add_test
# automatically assemble its targets into the unit dependency list.

cmake_host_system_information(RESULT N QUERY NUMBER_OF_LOGICAL_CORES)
if(NOT N EQUAL 0)
  math(EXPR NC "${N} / 2")
  if(${NC} GREATER 1)
    set(JFLAG "-j${NC}")
  else(${NC} GREATER 1)
    set(JFLAG)
  endif(${NC} GREATER 1)
else(NOT N EQUAL 0)
  # Huh?  No j flag if we can't get a core count
  set(JFLAG)
endif(NOT N EQUAL 0)

if(CMAKE_CONFIGURATION_TYPES)
  set(CONFIG $<CONFIG>)
else(CMAKE_CONFIGURATION_TYPES)
  if ("${CONFIG}" STREQUAL "")
    set(CONFIG "\"\"")
  endif ("${CONFIG}" STREQUAL "")
endif(CMAKE_CONFIGURATION_TYPES)

if (NOT TARGET check)
  add_custom_target(check
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \\\"check\\\" a.k.a. \\\"BRL-CAD Validation Testing\\\" target runs"
    COMMAND ${CMAKE_COMMAND} -E echo "      BRL-CAD\\'s unit, system, integration, benchmark \\(performance\\), and"
    COMMAND ${CMAKE_COMMAND} -E echo "      regression tests.  These tests must pass to consider a build viable"
    COMMAND ${CMAKE_COMMAND} -E echo "      for production use."
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -LE \"Regression|STAND_ALONE\" -E \"^regress-|NOTE|benchmark|slow-\" --output-on-failure ${JFLAG}
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -R \"benchmark\" --output-on-failure ${JFLAG}
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -L \"Regression\" --output-on-failure ${JFLAG}
    )
  set_target_properties(check PROPERTIES FOLDER "BRL-CAD Validation Testing")
endif (NOT TARGET check)

if (NOT TARGET unit)
  add_custom_target(unit
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \\\"unit\\\" a.k.a. \\\"BRL-CAD Unit Testing\\\" target runs all"
    COMMAND ${CMAKE_COMMAND} -E echo "      the BRL-CAD API unit tests that are expected to pass."
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -LE \"Regression|NOT_WORKING\" -E \"^regress-|NOTE|benchmark|slow-\" ${JFLAG}
    )
  set_target_properties(unit PROPERTIES FOLDER "BRL-CAD Validation Testing")
endif (NOT TARGET unit)

# we wrap the CMake add_test() function in order to automatically set up test
# dependencies for the 'unit' and 'check' test targets.
#
# this function extravagantly tries to work around a bug in CMake where we
# cannot pass an empty string through this wrapper to add_test().  passed as a
# list (e.g., via ARGN, ARGV, or manually composed), the empty string is
# skipped(!).  passed as a string, it is all treated as command name with no
# arguments.
#
# manual workaround used here involves invoking _add_test() with all args
# individually recreated/specified (i.e., not as a list) as this preserves
# empty strings.  this approach means we cannot generalize and only support a
# limited variety of empty string arguments, but we do test and halt if someone
# unknowingly tries.
function(BRLCAD_ADD_TEST NAME test_name COMMAND test_prog)

  # CMake 3.18, cmake_language based wrapper for add_test, replaces the
  # previous workaround for default ARGN behavior that doesn't pass through
  # empty strings.  See https://gitlab.kitware.com/cmake/cmake/-/issues/21414
  cmake_parse_arguments(PARSE_ARGV 3 ARG "" "" "")
  foreach(_av IN LISTS ARG_UNPARSED_ARGUMENTS)
    string(APPEND test_args " [==[${_av}]==]")
  endforeach()
  cmake_language(EVAL CODE "add_test(NAME ${test_name} COMMAND ${test_args})")

 # There are a variety of criteria that disqualify test_prog as a
  # dependency - check and return if we hit any of them.
  if (NOT TARGET ${test_prog})
    return()
  endif ()
  if ("${test_name}" MATCHES ^regress-)
    return()
  endif ()
  if ("${test_prog}" MATCHES ^regress-)
    return()
  endif ()
  if ("${test_name}" MATCHES ^slow-)
    return()
  endif ()
  if ("${test_name}" STREQUAL "benchmark")
    return()
  endif ()
  if ("${test_name}" MATCHES ^NOTE:)
    return()
  endif ()

  # add program needed for test to unit and check target dependencies
  add_dependencies(unit ${test_prog})
  add_dependencies(check ${test_prog})

endfunction(BRLCAD_ADD_TEST NAME test_name COMMAND test_prog)


# Local Variables:
# mode: cmake
# tab-width: 2
# indent-tabs-mode: nil
# End:
# ex: shiftwidth=2 tabstop=8
