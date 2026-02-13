set(PLUGIN_LIBS libged libdm libbu)

ged_plugin_library(ged-pix2fb
  pix2fb.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  pix2fb.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
