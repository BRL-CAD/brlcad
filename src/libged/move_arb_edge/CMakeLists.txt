include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-move_arb_edge SHARED move_arb_edge.c)
target_link_libraries(ged-move_arb_edge libged libbu)
set_property(TARGET ged-move_arb_edge APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-move_arb_edge move_arb_edge.c)
PLUGIN_SETUP(ged-move_arb_edge ged)

CMAKEFILES(
  CMakeLists.txt
  move_arb_edge.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
