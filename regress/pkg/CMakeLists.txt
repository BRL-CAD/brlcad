include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  )

if (TARGET libpkg)
  BRLCAD_ADDEXEC(regress_pkg regress_pkg.cpp "libpkg;libbu;Threads::Threads" NO_INSTALL)
  set_target_properties(regress_pkg PROPERTIES FOLDER "BRL-CAD Regression Tests/pkg")

  BRLCAD_REGRESSION_TEST(regress-pkg "regress_pkg" EXEC regress_pkg STAND_ALONE)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/regress-pkg.log)
endif (TARGET libpkg)

CMAKEFILES(
  CMakeLists.txt
  regress_pkg.cpp
  regress-pkg.cmake.in
  )
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
