include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

# Define the actual regression test
set(BRLCAD_EXEC_LIST "${CMAKE_CURRENT_BINARY_DIR}/brlcadexec.cmake")
set(BRLCAD_REGRESS_LIST "${CMAKE_CURRENT_BINARY_DIR}/regressfiles.cmake")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-coverage.log")

# List potential files of interest in the regress directory
file(GLOB_RECURSE REGRESS_FILES_TCL "${BRLCAD_SOURCE_DIR}/regress/*.tcl*")
file(GLOB_RECURSE REGRESS_FILES_SH "${BRLCAD_SOURCE_DIR}/regress/*.sh*")
file(GLOB_RECURSE REGRESS_FILES_CMAKE "${BRLCAD_SOURCE_DIR}/regress/*.cmake*")
file(REMOVE "${BRLCAD_REGRESS_LIST}")
foreach(rf ${REGRESS_FILES_TCL})
  file(APPEND "${BRLCAD_REGRESS_LIST}" "${rf}\n")
endforeach(rf ${REGRESS_FILES_TCL})
foreach(rf ${REGRESS_FILES_SH})
  file(APPEND "${BRLCAD_REGRESS_LIST}" "${rf}\n")
endforeach(rf ${REGRESS_FILES_SH})
foreach(rf ${REGRESS_FILES_CMAKE})
  file(APPEND "${BRLCAD_REGRESS_LIST}" "${rf}\n")
endforeach(rf ${REGRESS_FILES_CMAKE})

# Build a scanner to introspect the tests
BRLCAD_ADDEXEC(covgcheck covgcheck.cpp "libbu;Threads::Threads" TEST)
if (O3_COMPILER_FLAG)
  # This check benefits greatly from optimization. If we have the O3 flag, use
  # it - whether or not the standard build settings would add it.
  target_compile_options(covgcheck PRIVATE "-O3")
endif (O3_COMPILER_FLAG)
set_target_properties(covgcheck PROPERTIES FOLDER "BRL-CAD Regression Tests/repository")

#BRLCAD_REGRESSION_TEST(regress-coverage "covgcheck" EXEC covgcheck)
DISTCLEAN(${LOG_FILE})
DISTCLEAN(${BRLCAD_EXEC_LIST})
DISTCLEAN(${BRLCAD_REGRESS_LIST})

CMAKEFILES(
  CMakeLists.txt
  covgcheck.cpp
  regress-coverage.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
