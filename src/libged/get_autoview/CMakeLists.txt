include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-get_autoview SHARED get_autoview.c)
target_link_libraries(ged-get_autoview libged libdm libbu)
set_property(TARGET ged-get_autoview APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(ged-get_autoview get_autoview.c)
plugin_setup(ged-get_autoview ged)

cmakefiles(
  CMakeLists.txt
  get_autoview.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
