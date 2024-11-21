include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(
  CHECK_SRCS
  check_adj_air.c
  check.c
  check_centroid.c
  check_exp_air.c
  check_gap.c
  check_mass.c
  check_moments.c
  check_overlaps.c
  check_surf_area.c
  check_unconf_air.c
  check_volume.c
)

ged_plugin_library(ged-check ${CHECK_SRCS})
target_link_libraries(ged-check libged libbu)

cmakefiles(
  CMakeLists.txt
  ${CHECK_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
