include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-view2model_vec SHARED view2model_vec.c)
target_link_libraries(ged-view2model_vec libged libbu)
set_property(TARGET ged-view2model_vec APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-view2model_vec view2model_vec.c)
PLUGIN_SETUP(ged-view2model_vec ged)

CMAKEFILES(
  CMakeLists.txt
  view2model_vec.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
