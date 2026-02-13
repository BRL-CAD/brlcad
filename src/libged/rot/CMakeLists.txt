set(PLUGIN_LIBS libged libbu)

set(rot_srcs
  rot.c
  rotate_about.c
)

ged_plugin_library(ged-rot
  ${rot_srcs}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ${rot_srcs}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
