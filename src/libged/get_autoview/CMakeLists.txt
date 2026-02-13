set(PLUGIN_LIBS libged libdm libbu)

ged_plugin_library(ged-get_autoview
  get_autoview.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  get_autoview.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
