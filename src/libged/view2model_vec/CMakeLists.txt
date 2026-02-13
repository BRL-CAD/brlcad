set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-view2model_vec
  view2model_vec.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  view2model_vec.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
