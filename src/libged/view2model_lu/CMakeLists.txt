set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-view2model_lu
  view2model_lu.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  view2model_lu.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
