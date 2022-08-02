include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-pix2fb SHARED pix2fb.c)
target_link_libraries(ged-pix2fb libged libdm libbu)
set_property(TARGET ged-pix2fb APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-pix2fb pix2fb.c)
PLUGIN_SETUP(ged-pix2fb ged)

CMAKEFILES(
  CMakeLists.txt
  pix2fb.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
