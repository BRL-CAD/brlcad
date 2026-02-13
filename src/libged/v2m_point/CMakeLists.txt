set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-v2m_point
  v2m_point.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  v2m_point.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
