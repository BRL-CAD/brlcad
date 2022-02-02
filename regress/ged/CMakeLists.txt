# Tests for various GED commands

BRLCAD_INCLUDE_DIRS(GED_INCLUDE_DIRS)

BRLCAD_ADDEXEC(regress_mater mater.c libged TEST_USESDATA)
BRLCAD_REGRESSION_TEST(regress-ged_mater "regress_mater;ged-mater;density_DATAFILES_cp" EXEC regress_mater)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_mater.log")


BRLCAD_ADDEXEC(regress_push push.cpp libged TEST_USESDATA)
BRLCAD_REGRESSION_TEST(regress-ged_push "regress_push;ged-npush" EXEC regress_push)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_push.log")


CMAKEFILES(
  CMakeLists.txt
  ged_mater_test.g
  ged_push.g
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
