if (TARGET gcv-gdal)
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/hawaii-c.tar.bz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/hawaii-c)

  GCV_regress_util("dem" "hawaii-c" DEPS "gcv;gcv-gdal" GCV_OPTIONS "--input-format=gdal" EXPECTED_FILE_SIZE "2965640")
endif (TARGET gcv-gdal)

CMAKEFILES(
  CMakeLists.txt
  README
  hawaii-c.tar.bz2
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
