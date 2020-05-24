include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  )

BRLCAD_ADDEXEC(regress_pkg regress_pkg.cpp "libpkg;libbu;${CMAKE_THREAD_LIBS_INIT}" NO_INSTALL)
target_compile_definitions(regress_pkg PUBLIC BRLCADBUILD HAVE_CONFIG_H)
set_target_properties(regress_pkg PROPERTIES FOLDER "BRL-CAD Regression Tests/libpkg")

BRLCAD_REGRESSION_TEST(regress-pkg "regress_pkg" EXEC regress_pkg)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/regress-pkg.log)

CMAKEFILES(
  CMakeLists.txt
  )
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
