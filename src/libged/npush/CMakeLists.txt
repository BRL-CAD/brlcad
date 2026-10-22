set(PLUGIN_LIBS libged librt libbg libbu)

ged_plugin_library(ged-npush
  npush.cpp
  unpush.cpp
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  unpush.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
