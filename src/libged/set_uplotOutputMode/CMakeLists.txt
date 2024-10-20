include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-set_uplotOutputMode SHARED set_uplotOutputMode.c)
target_link_libraries(ged-set_uplotOutputMode libged libbu)
set_property(TARGET ged-set_uplotOutputMode APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(ged-set_uplotOutputMode set_uplotOutputMode.c)
plugin_setup(ged-set_uplotOutputMode ged)

cmakefiles(
  CMakeLists.txt
  set_uplotOutputMode.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
