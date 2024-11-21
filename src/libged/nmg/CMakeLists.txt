include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(
  NMG_SRCS
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

ged_plugin_library(ged-nmg ${NMG_SRCS})
target_link_libraries(ged-nmg libged libbu)

cmakefiles(
  CMakeLists.txt
  ${NMG_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
