# Tests for the various geometry format conversions supported
# by BRL-CAD

include_directories(${GED_INCLUDE_DIRS})

BRLCAD_ADDEXEC(regress_mater mater.c libged TEST_USESDATA)

BRLCAD_REGRESSION_TEST(regress-ged_mater "regress_mater;density_DATAFILES_cp" EXEC regress_mater)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/regress-ged_mater.log")

CMAKEFILES(
  CMakeLists.txt
  regress-ged_mater.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
