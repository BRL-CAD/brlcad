set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-ae2dir
  ae2dir.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ae2dir.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
