set(PLUGIN_LIBS libged libbu)

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

ged_plugin_library(ged-check
  ${CHECK_SRCS}
  LIBS ${PLUGIN_LIBS}
)

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
