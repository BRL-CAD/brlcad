if(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)
  include_directories(SYSTEM ${GDAL_INCLUDE_DIRS})
  LIBGCV_ADD_PLUGIN(gdal "gdal.cpp;gdal_ll.cpp" "librt;libbu;${GDAL_LIBRARIES}")
endif(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)

CMAKEFILES(gdal.cpp)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

