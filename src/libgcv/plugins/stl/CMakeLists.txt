set(STL_SRCS
  stl_read.c
  stl_write.c
  )

gcv_plugin_library(gcv-stl SHARED ${STL_SRCS})
target_link_libraries(gcv-stl libwdb librt)
set_property(TARGET gcv-stl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(gcv-stl "${STL_SRCS}")
PLUGIN_SETUP(gcv-stl gcv)

CMAKEFILES(
  ${STL_SRCS}
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

