set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-rt
  rt.c
  LIBS ${PLUGIN_LIBS}
  PLUGIN_ONLY
)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
