if(BRLCAD_ENABLE_GDAL)
  set(CMAKE_FIND_FRAMEWORK LAST) # bundled first
  brlcad_find_package(GDAL REQUIRED)
endif(BRLCAD_ENABLE_GDAL)

set(GDAL_SRCS gdal.cpp gdal_ll.cpp)

if(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)
  include_directories(SYSTEM ${GDAL_INCLUDE_DIR})

  gcv_plugin_library(gcv-gdal SHARED ${GDAL_SRCS})
  target_link_libraries(gcv-gdal libwdb librt libbu ${GDAL_LIBRARIES})
  set_property(TARGET gcv-gdal APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  validate_style(gcv-gdal "${GDAL_SRCS}")
  plugin_setup(gcv-gdal gcv)
endif(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)

cmakefiles(
  ${GDAL_SRCS}
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
