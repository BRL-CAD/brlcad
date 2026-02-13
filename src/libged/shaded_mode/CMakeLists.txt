set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-shaded_mode
  shaded_mode.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  shaded_mode.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
