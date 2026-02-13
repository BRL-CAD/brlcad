set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-pmodel2view
  pmodel2view.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  pmodel2view.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
