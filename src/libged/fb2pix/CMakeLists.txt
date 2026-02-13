set(PLUGIN_LIBS libged libdm libbu)

ged_plugin_library(ged-fb2pix
  fb2pix.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  fb2pix.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
