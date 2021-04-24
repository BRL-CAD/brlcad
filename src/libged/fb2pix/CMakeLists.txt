include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-fb2pix SHARED fb2pix.c)
target_link_libraries(ged-fb2pix libged libdm libbu)
set_property(TARGET ged-fb2pix APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-fb2pix fb2pix.c)
PLUGIN_SETUP(ged-fb2pix ged)

CMAKEFILES(
  CMakeLists.txt
  fb2pix.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
