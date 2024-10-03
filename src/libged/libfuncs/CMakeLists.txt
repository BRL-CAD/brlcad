include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-libfuncs SHARED libfuncs.c)
target_link_libraries(ged-libfuncs libged libbu)
set_property(TARGET ged-libfuncs APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(ged-libfuncs libfuncs.c)
plugin_setup(ged-libfuncs ged)

cmakefiles(
  CMakeLists.txt
  libfuncs.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
