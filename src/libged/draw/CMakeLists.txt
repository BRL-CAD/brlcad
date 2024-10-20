include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(DRAW_SRCS draw.c draw2.cpp loadview.c preview.c)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-draw SHARED ${DRAW_SRCS})
target_link_libraries(ged-draw libged libbu)
set_property(TARGET ged-draw APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(ged-draw ${DRAW_SRCS})
plugin_setup(ged-draw ged)

cmakefiles(
  CMakeLists.txt
  ${DRAW_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
