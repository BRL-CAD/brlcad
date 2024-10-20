include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-solids_on_ray SHARED solids_on_ray.c)
target_link_libraries(ged-solids_on_ray libged libbu)
set_property(TARGET ged-solids_on_ray APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(ged-solids_on_ray solids_on_ray.c)
plugin_setup(ged-solids_on_ray ged)

cmakefiles(
  CMakeLists.txt
  solids_on_ray.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
