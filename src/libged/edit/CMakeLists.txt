include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-edit SHARED "edit.c;edit2.cpp")
target_link_libraries(ged-edit libged libbu)
set_property(TARGET ged-edit APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(ged-edit "edit.c;edit2.cpp")
plugin_setup(ged-edit ged)

cmakefiles(
  CMakeLists.txt
  edit.c
  edit2.cpp
  ged_edit2.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
