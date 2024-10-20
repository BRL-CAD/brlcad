include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-garbage_collect SHARED garbage_collect.cpp)
target_link_libraries(ged-garbage_collect libged libbu)
set_property(TARGET ged-garbage_collect APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(ged-garbage_collect garbage_collect.cpp)
plugin_setup(ged-garbage_collect ged)

cmakefiles(
  CMakeLists.txt
  garbage_collect.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
