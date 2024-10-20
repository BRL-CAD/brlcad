if(TARGET libpkg)
  brlcad_addexec(regress_pkg regress_pkg.cpp "libpkg;libbu;Threads::Threads" TEST)
  if(TARGET regress_pkg)
    set_target_properties(regress_pkg PROPERTIES FOLDER "BRL-CAD Regression Tests/pkg")

    brlcad_regression_test(regress-pkg "regress_pkg" EXEC regress_pkg STAND_ALONE)
    distclean(${CMAKE_CURRENT_BINARY_DIR}/regress-pkg.log)
  endif(TARGET regress_pkg)
endif(TARGET libpkg)

cmakefiles(
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
