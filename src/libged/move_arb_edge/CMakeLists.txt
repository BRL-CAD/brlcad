set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-move_arb_edge
  move_arb_edge.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  move_arb_edge.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
