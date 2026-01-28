set(HELP_LIBS libged libbu)

ged_plugin_library(ged-help
  help.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${HELP_LIBS}
)

cmakefiles(
  CMakeLists.txt
  help.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
