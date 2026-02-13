set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-m2v_point
  m2v_point.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  m2v_point.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
