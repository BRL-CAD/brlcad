include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-model2grid_lu SHARED model2grid_lu.c)
target_link_libraries(ged-model2grid_lu libged libbu)
set_property(TARGET ged-model2grid_lu APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-model2grid_lu model2grid_lu.c)
PLUGIN_SETUP(ged-model2grid_lu ged)

CMAKEFILES(
  CMakeLists.txt
  model2grid_lu.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
