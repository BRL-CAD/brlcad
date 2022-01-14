#      B R L C A D _ T E S T _ W R A P P E R S . C M A K E
#
# BRL-CAD
#
# Copyright (c) 2020-2022 United States Government as represented by
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
    COMMAND ${CMAKE_COMMAND} -E echo "      regression tests.  To consider a build viable for production use,"
    COMMAND ${CMAKE_COMMAND} -E echo "      these tests must pass.  Dependencies are compiled automatically."
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
    COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \\\"unit\\\" a.k.a. \\\"BRL-CAD Unit Testing\\\" target runs the"
    COMMAND ${CMAKE_COMMAND} -E echo "      subset of BRL-CAD\\'s API unit tests that are expected to work \\(i.e.,"
    COMMAND ${CMAKE_COMMAND} -E echo "      tests not currently under development\\).  All dependencies required"
    COMMAND ${CMAKE_COMMAND} -E echo "      by the tests are compiled automatically."
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

  # TODO - once we can require CMake 3.18, replace the empty string workaround
  # below with this cmake_language based version.  See
  # https://gitlab.kitware.com/cmake/cmake/-/issues/21414

  # cmake_parse_arguments(PARSE_ARGV 3 ARG "" "" "")
  # foreach(_av IN LISTS ARG_UNPARSED_ARGUMENTS)
  #   string(APPEND test_args " [==[${_av}]==]")
  # endforeach()
  # cmake_language(EVAL CODE "add_test(NAME ${test_name} COMMAND ${test_args})")


  # find any occurrences of empty strings
  set(idx 0)
  set(matches)
  foreach (ARG IN LISTS ARGV)
    # need 'x' to avoid older cmake seeing "COMMAND" "STREQUAL" ""
    if ("x${ARG}" STREQUAL "x")
      list(APPEND matches ${idx})
    endif ("x${ARG}" STREQUAL "x")
    math(EXPR idx "${idx} + 1")
  endforeach()

  # make sure we don't exceed current support
  list(LENGTH matches cnt)
  if ("${cnt}" GREATER 1)
    message(FATAL_ERROR "ERROR: encountered ${cnt} > 1 empty string being passed to add_test(${test_name}).  Expand support in the top-level CMakeLists.txt file (grep add_test) or pass fewer empty strings.")
  endif ("${cnt}" GREATER 1)

  # if there are empty strings, we need to manually recreate their calling
  if ("${cnt}" GREATER 0)

    list(GET matches 0 empty)
    if ("${empty}" EQUAL 4)
      foreach (i 1)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} "" ${ARGN})
    elseif ("${empty}" EQUAL 5)
      foreach (i 1 2)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} "" ${ARGN})
    elseif ("${empty}" EQUAL 6)
      foreach (i 1 2 3)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} "" ${ARGN})
    elseif ("${empty}" EQUAL 7)
      foreach (i 1 2 3 4)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} "" ${ARGN})
    elseif ("${empty}" EQUAL 8)
      foreach (i 1 2 3 4 5)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} "" ${ARGN})
    elseif ("${empty}" EQUAL 9)
      foreach (i 1 2 3 4 5 6)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} "" ${ARGN})
    elseif ("${empty}" EQUAL 10)
      foreach (i 1 2 3 4 5 6 7)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} ${ARGV9} "" ${ARGN})
    elseif ("${empty}" EQUAL 11)
      foreach (i 1 2 3 4 5 6 7 8)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} ${ARGV9} ${ARGV10} "" ${ARGN})


      # ADD_EMPTY_HERE: insert support for additional argv positions
      # as extra elseif tests here using the preceding pattern.  be
      # sure to update the index in the following else clause fatal
      # error message too.

    else ("${empty}" EQUAL 4)
      message(FATAL_ERROR "ERROR: encountered an empty string passed to add_test(${test_name}) as ARGV${empty} > ARGV11.  Expand support in the top-level CMakeLists.txt file (grep ADD_EMPTY_HERE).")
    endif ("${empty}" EQUAL 4)

  else ("${cnt}" GREATER 0)
    # no empty strings, no worries
    add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGN})
  endif ("${cnt}" GREATER 0)


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
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
