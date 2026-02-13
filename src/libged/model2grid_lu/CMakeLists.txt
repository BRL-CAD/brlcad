set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-model2grid_lu
  model2grid_lu.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  model2grid_lu.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
