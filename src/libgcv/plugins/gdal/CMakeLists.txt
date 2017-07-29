find_package(GDAL)

if(GDAL_FOUND)
  include_directories(SYSTEM ${GDAL_INCLUDE_DIR})
  LIBGCV_ADD_PLUGIN(gdal "gdal.cpp" "librt;libbu;${GDAL_LIBRARY}")
endif(GDAL_FOUND)

CMAKEFILES(gdal.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

