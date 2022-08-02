if (TARGET gcv-gdal)
  BRLCAD_REGRESSION_TEST(regress-dem "gcv;gcv-gdal" EXEC gcv)
endif (TARGET gcv-gdal)

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
