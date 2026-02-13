set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-move_arb_face
  move_arb_face.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  move_arb_face.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
