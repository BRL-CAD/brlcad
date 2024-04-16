include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

#############################################
#         Embedded license files
#############################################

# Build scanner to read license information and introspect the source tree
BRLCAD_ADDEXEC(echeck embedded_check.cpp "" TEST)
if (O3_COMPILER_FLAG)
  # This check benefits greatly from optimization. If we have the O3 flag, use
  # it - whether or not the standard build settings would add it.
  target_compile_options(echeck PRIVATE "-O3")
endif (O3_COMPILER_FLAG)
set_target_properties(echeck PROPERTIES FOLDER "BRL-CAD Regression Tests/licenses")

# Get the list of embedded license files from doc/legal/embedded and generate
# an input file for the regression test
file(GLOB embedded_licenses "${CMAKE_SOURCE_DIR}/doc/legal/embedded/*.txt")
list(REMOVE_ITEM embedded_licenses "${CMAKE_SOURCE_DIR}/doc/legal/embedded/CMakeLists.txt")
set(LICENSE_LIST "${CMAKE_CURRENT_BINARY_DIR}/embedded_licenses.txt")
file(REMOVE "${LICENSE_LIST}")
foreach(lfile ${embedded_licenses})
  file(APPEND "${LICENSE_LIST}" "${lfile}\n")
endforeach(lfile ${embedded_licenses})
DISTCLEAN("${LICENSE_LIST}")

# Define the actual regression test
set(ALL_FILES_LIST "${BRLCAD_BINARY_DIR}/cmakefiles.cmake")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-licenses.log")
BRLCAD_REGRESSION_TEST(regress-licenses "echeck" EXEC echeck)
DISTCLEAN(${LOG_FILE})

CMAKEFILES(
  CMakeLists.txt
  embedded_check.cpp
  regress-licenses.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
