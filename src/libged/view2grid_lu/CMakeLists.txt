include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-view2grid_lu SHARED view2grid_lu.c)
target_link_libraries(ged-view2grid_lu libged libbu)
set_property(TARGET ged-view2grid_lu APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-view2grid_lu view2grid_lu.c)
PLUGIN_SETUP(ged-view2grid_lu ged)

CMAKEFILES(
  CMakeLists.txt
  view2grid_lu.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
