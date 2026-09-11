#      B R L C A D _ T E S T _ W R A P P E R S . C M A K E
#
# BRL-CAD
#
# Copyright (c) 2020-2026 United States Government as represented by
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

if("${CONFIG}" STREQUAL "")
  set(CONFIG "\"${CMAKE_BUILD_TYPE}\"")
endif("${CONFIG}" STREQUAL "")

if(NOT TARGET check)
  add_custom_target(
    check
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \\\"check\\\" a.k.a. \\\"BRL-CAD Validation Testing\\\" target runs"
    COMMAND ${CMAKE_COMMAND} -E echo "      BRL-CAD\\'s unit, system, integration, benchmark \\(performance\\), and"
    COMMAND ${CMAKE_COMMAND} -E echo "      regression tests.  These tests must pass to consider a build viable"
    COMMAND ${CMAKE_COMMAND} -E echo "      for production use."
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND
      ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -LE \"Regression|STAND_ALONE\" -E \"^regress-|NOTE|benchmark|slow-\"
      --output-on-failure ${JFLAG}
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -R \"benchmark\" --output-on-failure ${JFLAG}
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -L \"Regression\" --output-on-failure ${JFLAG}
  )
  set_target_properties(check PROPERTIES FOLDER "BRL-CAD Validation Testing")
endif(NOT TARGET check)

if(NOT TARGET unit)
  add_custom_target(
    unit
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \\\"unit\\\" a.k.a. \\\"BRL-CAD Unit Testing\\\" target runs all"
    COMMAND ${CMAKE_COMMAND} -E echo "      the BRL-CAD API unit tests that are expected to pass."
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND
      ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -LE \"Regression|NOT_WORKING\" -E \"^regress-|NOTE|benchmark|slow-\" ${JFLAG}
  )
  set_target_properties(unit PROPERTIES FOLDER "BRL-CAD Validation Testing")
endif(NOT TARGET unit)

# We wrap the CMake add_test() function in order to automatically set up test
# dependencies for the 'unit' and 'check' test targets.
function(BRLCAD_ADD_TEST NAME test_name COMMAND test_prog)
  # If the user is telling us tests are disabled, nothing to do
  if(NOT BUILD_TESTING)
    return()
  endif(NOT BUILD_TESTING)

  # Assemble the evaluated add_test call directly from ARGVn.  Converting the
  # arguments to an ordinary CMake list drops empty fields, which changes the
  # position of later command arguments.  See
  # https://gitlab.kitware.com/cmake/cmake/-/issues/21414
  set(_test_command "${test_prog}")
  # Once a launcher precedes the test command, add_test no longer recognizes a
  # bare target name as its executable and consequently does not replace it
  # with the target's path.  Resolve target-backed commands explicitly while
  # preserving literal commands and generator expressions.
  if(BRLCAD_SANITIZER_TEST_LAUNCHER AND TARGET ${test_prog})
    set(_test_command "$<TARGET_FILE:${test_prog}>")
  endif()
  string(APPEND test_args " [==[${_test_command}]==]")
  if(ARGC GREATER 4)
    math(EXPR _last_test_arg "${ARGC} - 1")
    foreach(_arg_index RANGE 4 ${_last_test_arg})
      string(APPEND test_args " [==[${ARGV${_arg_index}}]==]")
    endforeach()
  endif()
  foreach(_launcher_arg IN LISTS BRLCAD_SANITIZER_TEST_LAUNCHER)
    string(APPEND test_launcher_args " [==[${_launcher_arg}]==]")
  endforeach()
  cmake_language(EVAL CODE
    "add_test(NAME ${test_name} COMMAND ${test_launcher_args} ${test_args})"
  )

  # Collect both a directly invoked target and targets referenced by script
  # arguments.  The latter commonly appear as -DTOOL=$<TARGET_FILE:tool>;
  # without these dependencies Ninja may start check before those tools exist.
  set(_test_dependencies)
  if(TARGET ${test_prog})
    list(APPEND _test_dependencies ${test_prog})
  endif()
  if(ARGC GREATER 4)
    foreach(_arg_index RANGE 4 ${_last_test_arg})
      set(_test_arg "${ARGV${_arg_index}}")
      if(_test_arg MATCHES "\\$<TARGET_FILE:([^>]+)>")
        set(_test_arg_target "${CMAKE_MATCH_1}")
        if(TARGET ${_test_arg_target})
          list(APPEND _test_dependencies ${_test_arg_target})
        endif()
      endif()
    endforeach()
  endif()
  if(NOT _test_dependencies)
    return()
  endif()
  list(REMOVE_DUPLICATES _test_dependencies)

  # There are a variety of criteria that disqualify test programs as
  # dependencies - check and return if we hit any of them.
  if("${test_name}" MATCHES ^regress-)
    return()
  endif()
  if("${test_prog}" MATCHES ^regress-)
    return()
  endif()
  if("${test_name}" MATCHES ^slow-)
    return()
  endif()
  if("${test_name}" STREQUAL "benchmark")
    return()
  endif()
  if("${test_name}" MATCHES ^NOTE:)
    return()
  endif()

  # Add programs needed for the test to unit and check target dependencies.
  add_dependencies(unit ${_test_dependencies})
  add_dependencies(check ${_test_dependencies})
endfunction(BRLCAD_ADD_TEST)

# Register a test whose command needs a private X server.  Keeping Xmin's
# process setup here lets GUI fixtures concentrate on application behavior and
# gives every caller the same display geometry, timeout, and dependency
# handling.  Tests are registered only when the build selected Xmin as its X11
# provider.
function(BRLCAD_ADD_XMIN_TEST)
  if(NOT BUILD_TESTING OR NOT BRLCAD_X11_PROVIDER_RESOLVED STREQUAL "XMIN")
    return()
  endif()

  cmake_parse_arguments(PARSE_ARGV 0 XMIN "" "NAME;SCREEN;TIMEOUT"
    "COMMAND;ENVIRONMENT;SERVER_ARGS")
  if(NOT XMIN_NAME)
    message(FATAL_ERROR "BRLCAD_ADD_XMIN_TEST requires NAME")
  endif()
  if(NOT XMIN_COMMAND)
    message(FATAL_ERROR "BRLCAD_ADD_XMIN_TEST requires COMMAND")
  endif()

  set(_xmin_server "${Xmin_SERVER_EXECUTABLE}")
  if(NOT _xmin_server AND TARGET Xmin::server)
    get_target_property(_xmin_server Xmin::server IMPORTED_LOCATION)
  endif()

  set(_xmin_run "${Xmin_RUN_EXECUTABLE}")
  if(NOT _xmin_run AND _xmin_server)
    get_filename_component(_xmin_bin_dir "${_xmin_server}" DIRECTORY)
    find_program(_xmin_run NAMES xmin-run HINTS "${_xmin_bin_dir}" NO_DEFAULT_PATH NO_CACHE)
  endif()
  if(NOT _xmin_server OR NOT _xmin_run)
    message(WARNING "Skipping ${XMIN_NAME}: the selected Xmin SDK does not provide its test runner")
    return()
  endif()

  if(NOT XMIN_SCREEN)
    set(XMIN_SCREEN "1280x960x24")
  endif()
  if(NOT _xmin_bin_dir)
    get_filename_component(_xmin_bin_dir "${_xmin_server}" DIRECTORY)
  endif()
  find_program(_xmin_ctl NAMES xminctl HINTS "${_xmin_bin_dir}"
    NO_DEFAULT_PATH NO_CACHE)

  set(_xmin_environment "XMIN_SCREEN=${XMIN_SCREEN}" ${XMIN_ENVIRONMENT})
  if(_xmin_ctl)
    list(PREPEND _xmin_environment "XMIN_CTL=${_xmin_ctl}")
  endif()
  set(_xmin_command
    "${CMAKE_COMMAND}" -E env ${_xmin_environment} ${XMIN_COMMAND}
  )

  brlcad_add_test(
    NAME ${XMIN_NAME}
    COMMAND "${_xmin_run}" --server "${_xmin_server}"
      --screen "${XMIN_SCREEN}" ${XMIN_SERVER_ARGS} -- ${_xmin_command}
  )
  if(XMIN_TIMEOUT)
    set_tests_properties(${XMIN_NAME} PROPERTIES TIMEOUT "${XMIN_TIMEOUT}")
  endif()
endfunction(BRLCAD_ADD_XMIN_TEST)

# Local Variables:
# mode: cmake
# tab-width: 2
# indent-tabs-mode: nil
# End:
# ex: shiftwidth=2 tabstop=8
