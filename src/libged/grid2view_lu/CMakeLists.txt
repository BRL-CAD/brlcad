set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-grid2view_lu
  grid2view_lu.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  grid2view_lu.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
