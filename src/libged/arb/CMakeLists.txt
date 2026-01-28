set(PLUGIN_LIBS libged libbu)

set(arb_srcs arb.c rotate_arb_face.c)

ged_plugin_library(ged-arb
  ${arb_srcs}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ${arb_srcs}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
