include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(NMG_SRCS
  nmg.c
  nmg_cmface.c
  nmg_collapse.c
  nmg_fix_normals.c
  nmg_kill_f.c
  nmg_kill_v.c
  nmg_make_v.c
  nmg_mm.c
  nmg_move_v.c
  nmg_simplify.c
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-nmg SHARED ${NMG_SRCS})
target_link_libraries(ged-nmg libged libbu)
set_property(TARGET ged-nmg APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-nmg ${NMG_SRCS})
PLUGIN_SETUP(ged-nmg ged)

CMAKEFILES(
  CMakeLists.txt
  ${NMG_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

