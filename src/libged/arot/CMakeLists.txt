set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-arot
  arot.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  arot.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
