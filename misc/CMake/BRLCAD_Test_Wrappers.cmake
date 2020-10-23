
# "make check" runs all of the tests (unit, benchmark, and regression) that are expected to work.
include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
	math(EXPR NC "${N} / 2")
	if(${NC} GREATER 1)
		set(JFLAG "-j${NC}")
	else(${NC} GREATER 1)
		set(JFLAG)
	endif(${NC} GREATER 1)
else(NOT N EQUAL 0)
	# Huh?  No j flag if we can't get a processor count
	set(JFLAG)
endif(NOT N EQUAL 0)

if(CMAKE_CONFIGURATION_TYPES)
	set(CONFIG $<CONFIG>)
else(CMAKE_CONFIGURATION_TYPES)
	if ("${CONFIG}" STREQUAL "")
		set(CONFIG "\"\"")
	endif ("${CONFIG}" STREQUAL "")
endif(CMAKE_CONFIGURATION_TYPES)

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

# To support "make unit" (which will build the required targets for testing
# in the style of GNU Autotools "make check") we define a "unit" target per
# http://www.cmake.org/Wiki/CMakeEmulateMakeCheck and have add_test
# automatically assemble its targets into the unit dependency list.

if(CMAKE_CONFIGURATION_TYPES)
	set(CONFIG $<CONFIG>)
else(CMAKE_CONFIGURATION_TYPES)
	if ("${CONFIG}" STREQUAL "")
		set(CONFIG "\"\"")
	endif ("${CONFIG}" STREQUAL "")
endif(CMAKE_CONFIGURATION_TYPES)

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

# we wrap the CMake add_test() function in order to automatically
# set up test dependencies for the 'unit' and 'check' test targets.
#
# this function extravagantly tries to work around a bug in CMake
# where we cannot pass an empty string through this wrapper to
# _add_test()/add_test().  passed as a list (e.g., via ARGN, ARGV,
# or manually composed), the empty string is skipped(!).  passed as
# a string, it is all treated as command name with no arguments.
#
# manual workaround used here involves invoking _add_test() with all
# args individually recreated/specified (i.e., not as a list) as
# this preserves empty strings.  this approach means we cannot
# generalize and only support a limited variety of empty string
# arguments, but we do test and halt if someone unknowingly tries.
function(add_test NAME test_name COMMAND test_prog)

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
			_add_test(NAME ${test_name} COMMAND ${test_prog} "" ${ARGN})
		elseif ("${empty}" EQUAL 5)
			foreach (i 1 2)
				if (ARGN)
					list(REMOVE_AT ARGN 0)
				endif (ARGN)
			endforeach ()
			_add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} "" ${ARGN})
		elseif ("${empty}" EQUAL 6)
			foreach (i 1 2 3)
				if (ARGN)
					list(REMOVE_AT ARGN 0)
				endif (ARGN)
			endforeach ()
			_add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} "" ${ARGN})
		elseif ("${empty}" EQUAL 7)
			foreach (i 1 2 3 4)
				if (ARGN)
					list(REMOVE_AT ARGN 0)
				endif (ARGN)
			endforeach ()
			_add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} "" ${ARGN})
		elseif ("${empty}" EQUAL 8)
			foreach (i 1 2 3 4 5)
				if (ARGN)
					list(REMOVE_AT ARGN 0)
				endif (ARGN)
			endforeach ()
			_add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} "" ${ARGN})
		elseif ("${empty}" EQUAL 9)
			foreach (i 1 2 3 4 5 6)
				if (ARGN)
					list(REMOVE_AT ARGN 0)
				endif (ARGN)
			endforeach ()
			_add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} "" ${ARGN})
		elseif ("${empty}" EQUAL 10)
			foreach (i 1 2 3 4 5 6)
				if (ARGN)
					list(REMOVE_AT ARGN 0)
				endif (ARGN)
			endforeach ()
			_add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} ${ARGV9} "" ${ARGN})
		elseif ("${empty}" EQUAL 11)
			foreach (i 1 2 3 4 5 6)
				if (ARGN)
					list(REMOVE_AT ARGN 0)
				endif (ARGN)
			endforeach ()
			_add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} ${ARGV9} ${ARGV10} "" ${ARGN})


			# ADD_EMPTY_HERE: insert support for addition argv positions
			# as extra elseif tests here using the preceding pattern.  be
			# sure to update the index in the following else clause fatal
			# error message too.

		else ("${empty}" EQUAL 4)
			message(FATAL_ERROR "ERROR: encountered an empty string passed to add_test(${test_name}) as ARGV${empty} > ARGV9.  Expand support in the top-level CMakeLists.txt file (grep ADD_EMPTY_HERE).")
		endif ("${empty}" EQUAL 4)

	else ("${cnt}" GREATER 0)
		# no empty strings, no worries
		_add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGN})
	endif ("${cnt}" GREATER 0)

	# add test to unit and check targets
	if (NOT "${test_name}" MATCHES ^regress- AND NOT "${test_prog}" MATCHES ^regress- AND NOT "${test_name}" MATCHES ^slow- AND NOT "${test_name}" STREQUAL "benchmark" AND NOT "${test_name}" MATCHES ^NOTE:)
		add_dependencies(unit ${test_prog})
		add_dependencies(check ${test_prog})
	endif (NOT "${test_name}" MATCHES ^regress- AND NOT "${test_prog}" MATCHES ^regress- AND NOT "${test_name}" MATCHES ^slow- AND NOT "${test_name}" STREQUAL "benchmark" AND NOT "${test_name}" MATCHES ^NOTE:)

endfunction(add_test NAME test_name COMMAND test_prog)


