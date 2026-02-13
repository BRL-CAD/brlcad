set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-set_transparency
  set_transparency.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  set_transparency.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
