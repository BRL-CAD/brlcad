include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
add_library(ged-nmg_fix_normals SHARED nmg_fix_normals.c)
target_link_libraries(ged-nmg_fix_normals libged libbu)
set_property(TARGET ged-nmg_fix_normals APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-nmg_fix_normals nmg_fix_normals.c)
PLUGIN_SETUP(ged-nmg_fix_normals ged)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
