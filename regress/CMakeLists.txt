# Define the toplevel regression target - unless specifically excluded
# via a macro argument, this should collect all regression targets.
include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0 AND NOT BRLCAD_SERIAL_TESTING)
  set(JFLAG "-j${N}")
else()
  set(JFLAG)
endif()
if (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_CFG_INTDIR} -L Regression --output-on-failure --output-log "${CMAKE_BINARY_DIR}/regress_output.log" ${JFLAG})
else (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress COMMAND ${CMAKE_CTEST_COMMAND} -L Regression --output-on-failure --output-log "${CMAKE_BINARY_DIR}/regress_output.log" ${JFLAG})
endif (CMAKE_CONFIGURATION_TYPES)
DISTCLEAN(${CMAKE_BINARY_DIR}/regress_output.log)
set_target_properties(regress PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress PROPERTIES FOLDER "BRL-CAD Regression Tests")

# ASC file Conversion Tests
add_subdirectory(asc)

# Bag of Triangles Tests
add_subdirectory(bots)

# Burst Regression Tests
add_subdirectory(burst)

# comgeom Conversion Tests
add_subdirectory(comgeom)

# Coverage Tests
add_subdirectory(coverage)

# DSP Regression Tests
add_subdirectory(dsp)

# Fuzz tests (if supported by the compiler)
add_subdirectory(fuzz)

# Geometry difference reporting
add_subdirectory(gdiff)

# Image Conversion Regression Tests
add_subdirectory(icv)

# gchecker tests
add_subdirectory(gchecker)

# Geometry Editing Library Regression Tests
add_subdirectory(ged)

# Geometry Conversion Regression Tests
add_subdirectory(gcv)

# GQA Regression Tests
add_subdirectory(gqa)

# IGES Regression Tests
add_subdirectory(iges)

# License check
add_subdirectory(licenses)

# light tests
add_subdirectory(lights)

# MGED command tests
add_subdirectory(mged)

# moss.g Regression Tests
add_subdirectory(moss)

# NIRT Regression Tests
add_subdirectory(nirt)

# NURBS Regression Tests
add_subdirectory(nurbs)

# libpkg Regression Tests
add_subdirectory(pkg)

# Region EDit (red) Regression Tests
add_subdirectory(red)

# Repository check
add_subdirectory(repository)

# rtedge Regression Tests
add_subdirectory(rtedge)

# RtWizard Image Generation Regression Tests
add_subdirectory(rtwizard)

# Simulation of a 3rd party BRL-CAD library client code
add_subdirectory(user)

# solids Regression Tests
add_subdirectory(solids)

# spdi Regression Tests
add_subdirectory(spdi)

# shaders Regression Tests
add_subdirectory(shaders)

# rtweight command tests
add_subdirectory(weight)

if(SH_EXEC)

  BRLCAD_ADD_TEST(NAME regress-usage COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/usage.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-usage "rt" STAND_ALONE TEST_DEFINED)

  # TODO - test for Python before adding this one
  BRLCAD_ADD_TEST(NAME regress-flawfinder COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/flawfinder.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-flawfinder "" STAND_ALONE TEST_DEFINED)

endif(SH_EXEC)

CMAKEFILES(
  art.sh
  flawfinder.sh
  g-dot.sh
  g-nff.sh
  library.sh
  rtweight.sh
  usage.sh
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
