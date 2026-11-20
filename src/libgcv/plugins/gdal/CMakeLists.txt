if(BRLCAD_ENABLE_GDAL)
  set(CMAKE_FIND_FRAMEWORK LAST) # bundled first
  brlcad_find_package(GDAL)
  if (NOT GDAL_LIBRARY)
    message("BRLCAD_ENABLE_GDAL is on, but GDAL not found - disabling.")
    set(BRLCAD_ENABLE_GDAL OFF)
  endif()
endif(BRLCAD_ENABLE_GDAL)

set(GDAL_SRCS gdal.cpp gdal_ll.cpp)

if(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)
  find_package_zlib(REQUIRED)
  include_directories(SYSTEM ${GDAL_INCLUDE_DIR})

  gcv_plugin_library(gcv-gdal SHARED ${GDAL_SRCS})
  target_link_libraries(gcv-gdal libwdb librt libbu ${GDAL_LIBRARIES} ZLIB::ZLIB)
  set_property(TARGET gcv-gdal APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  validate_style(gcv-gdal "${GDAL_SRCS}")
  plugin_setup(gcv-gdal gcv)
endif(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)

cmakefiles(
  ${GDAL_SRCS}
  format_map.h
  hf2_mime.h
  raster_mime.h
  photomod_mime.h
  esric_mime.h
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
