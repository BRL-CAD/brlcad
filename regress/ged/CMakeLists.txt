# Tests for various GED commands

if(TARGET libged)
  brlcad_addexec(regress_mater mater.c libged TEST_USESDATA)
  brlcad_regression_test(regress-ged_mater "regress_mater;ged-mater;density_DATAFILES_cp" EXEC regress_mater)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_mater.log")
  distclean("${CMAKE_CURRENT_BINARY_DIR}/ged_mater_test.g")

  brlcad_addexec(regress_push push.cpp libged TEST_USESDATA)
  brlcad_regression_test(regress-ged_push "regress_push;ged-npush" EXEC regress_push)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_push.log")
  distclean("${CMAKE_CURRENT_BINARY_DIR}/ged_push.g")
endif(TARGET libged)

cmakefiles(
  CMakeLists.txt
  mater.c
  push.cpp
  ppush_tests.g
  push_tests.g
  regress-ged_mater.cmake.in
  regress-ged_push.cmake.in
  xpush_tests.g
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
