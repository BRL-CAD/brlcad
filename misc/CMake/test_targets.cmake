# Get information necessary for target definitions
include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
  set(JFLAG "-j${N}")
else(NOT N EQUAL 0)
  # Huh?  No j flag if we can't get a processor count
  set(JFLAG)
endif(NOT N EQUAL 0)

if(${CMAKE_VERSION} VERSION_GREATER 2.99)
  set(CONFIG $<CONFIG>)
else(${CMAKE_VERSION} VERSION_GREATER 2.99)
  set(CONFIG $<CONFIGURATION>)
endif(${CMAKE_VERSION} VERSION_GREATER 2.99)


# "make check" runs all of the tests (unit, benchmark, and regression) that are expected to work.
add_custom_target(check
  COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
  COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \\\"check\\\" a.k.a. \\\"BRL-CAD Validation Testing\\\" target runs"
  COMMAND ${CMAKE_COMMAND} -E echo "      BRL-CAD\\'s unit, system, integration, benchmark \\(performance\\), and"
  COMMAND ${CMAKE_COMMAND} -E echo "      regression tests.  To consider a build viable for production use,"
  COMMAND ${CMAKE_COMMAND} -E echo "      these tests must pass.  Dependencies are compiled automatically."
  COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
  COMMAND ${CMAKE_CTEST_COMMAND} -LE \"Regression|NOT_WORKING\" -E \"^regress-|NOTE|benchmark\" ${JFLAG}
  COMMAND ${CMAKE_CTEST_COMMAND} -R \"benchmark\" --output-on-failure ${JFLAG}
  COMMAND ${CMAKE_CTEST_COMMAND} -L \"Regression\" --output-on-failure ${JFLAG}
  )
set_target_properties(check PROPERTIES FOLDER "BRL-CAD Validation Testing")


# To support "make unit" (which will build the required targets for testing
# in the style of GNU Autotools "make check") we define a "unit" target per
# http://www.cmake.org/Wiki/CMakeEmulateMakeCheck and have add_test
# automatically assemble its targets into the unit dependency list.
add_custom_target(unit
  COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
  COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \"unit\" a.k.a. \"BRL-CAD Unit Testing\" target runs the"
  COMMAND ${CMAKE_COMMAND} -E echo "      subset of BRL-CAD's API unit tests that are expected to work (i.e.,"
  COMMAND ${CMAKE_COMMAND} -E echo "      tests not currently under development).  All dependencies required"
  COMMAND ${CMAKE_COMMAND} -E echo "      by the tests are compiled automatically."
  COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
  COMMAND ${CMAKE_CTEST_COMMAND} -C $CONFIG -LE \"Regression|NOT_WORKING\" -E \"^regress-|NOTE\" ${JFLAG}
  )
set_target_properties(unit PROPERTIES FOLDER "BRL-CAD Validation Testing")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

