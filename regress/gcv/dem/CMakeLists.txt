BRLCAD_REGRESSION_TEST(regress-dem "gcv;libgcv_gdal" EXEC gcv)

DISTCLEAN(
  "${CMAKE_CURRENT_BINARY_DIR}/regress-dem.log"
  "${CMAKE_CURRENT_BINARY_DIR}/hawaii-c"
  "${CMAKE_CURRENT_BINARY_DIR}/hawaii-c.g"
  )

CMAKEFILES(
  CMakeLists.txt
  README
  hawaii-c.tar.bz2
  regress-dem.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
