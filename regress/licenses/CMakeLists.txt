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
get_directory_property(ELEGAL DIRECTORY ${CMAKE_SOURCE_DIR}/doc/legal/embedded DEFINITION embedded_licenses)
set(LICENSE_LIST "${CMAKE_CURRENT_BINARY_DIR}/embedded_licenses.txt")
file(REMOVE "${LICENSE_LIST}")
foreach(lfile ${ELEGAL})
  file(APPEND "${LICENSE_LIST}" "${CMAKE_SOURCE_DIR}/doc/legal/embedded/${lfile}\n")
endforeach(lfile ${ELEGAL})
DISTCLEAN("${LICENSE_LIST}")

# Define the actual regression test
set(ALL_FILES_LIST "${BRLCAD_BINARY_DIR}/cmakefiles.cmake")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-licenses.log")
BRLCAD_REGRESSION_TEST(regress-licenses "echeck" EXEC echeck)
DISTCLEAN(${LOG_FILE})

#############################################
#         src/other license files
#############################################

# Build scanner to read license information
BRLCAD_ADDEXEC(ocheck other_check.cpp "" TEST)
if (O3_COMPILER_FLAG)
  # This check benefits greatly from optimization. If we have the O3 flag, use
  # it - whether or not the standard build settings would add it.
  target_compile_options(ocheck PRIVATE "-O3")
endif (O3_COMPILER_FLAG)
set_target_properties(ocheck PROPERTIES FOLDER "BRL-CAD Regression Tests/licenses")

# Get the list of other license files from doc/legal/other and generate
# an input file for the regression test
get_directory_property(OLEGAL DIRECTORY ${CMAKE_SOURCE_DIR}/doc/legal/other DEFINITION other_licenses)
set(LICENSE_LIST "${CMAKE_CURRENT_BINARY_DIR}/other_licenses.txt")
file(REMOVE "${LICENSE_LIST}")
foreach(lfile ${OLEGAL})
  file(APPEND "${LICENSE_LIST}" "${CMAKE_SOURCE_DIR}/doc/legal/other/${lfile}\n")
endforeach(lfile ${OLEGAL})
DISTCLEAN("${LICENSE_LIST}")

# Get the list of dist files
file(GLOB OTHER_DIST "${CMAKE_SOURCE_DIR}/src/other/*.dist")
file(GLOB EXT_DIST "${CMAKE_SOURCE_DIR}/src/other/ext/*.dist")
set(DIST_LIST "${CMAKE_CURRENT_BINARY_DIR}/dist_files.txt")
file(REMOVE "${DIST_LIST}")
foreach(dfile ${OTHER_DIST})
  file(APPEND "${DIST_LIST}" "${dfile}\n")
endforeach(dfile ${OTHER_DIST})
foreach(dfile ${EXT_DIST})
  file(APPEND "${DIST_LIST}" "${dfile}\n")
endforeach(dfile ${EXT_DIST})
DISTCLEAN("${DIST_LIST}")

# Define the actual regression test
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-other_licenses.log")
BRLCAD_REGRESSION_TEST(regress-other_licenses "ocheck" EXEC ocheck)
DISTCLEAN(${LOG_FILE})


CMAKEFILES(
  CMakeLists.txt
  embedded_check.cpp
  other_check.cpp
  regress-licenses.cmake.in
  regress-other_licenses.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
