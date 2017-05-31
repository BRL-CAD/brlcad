find_package(GDAL)

if(GDAL_FOUND)
  include_directories(SYSTEM ${GDAL_INCLUDE_DIR})
  LIBGCV_ADD_PLUGIN(gdal "gdal.cpp" "librt;libbu;${GDAL_LIBRARY}")
else(GDAL_FOUND)
  CMAKEFILES(../gdal)
endif(GDAL_FOUND)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

