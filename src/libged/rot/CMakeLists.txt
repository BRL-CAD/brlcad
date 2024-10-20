include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(rot_srcs rot.c rotate_about.c)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-rot SHARED ${rot_srcs})
target_link_libraries(ged-rot libged libbu)
set_property(TARGET ged-rot APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(ged-rot ${rot_srcs})
plugin_setup(ged-rot ged)

cmakefiles(
  CMakeLists.txt
  ${rot_srcs}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
