set(STL_SRCS
  stl_read.c
  stl_write.cpp
  )

gcv_plugin_library(gcv-stl SHARED ${STL_SRCS})
target_link_libraries(gcv-stl libgcv libwdb librt)
set_property(TARGET gcv-stl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-stl "${STL_SRCS}")
plugin_setup(gcv-stl gcv)

cmakefiles(
  ${STL_SRCS}
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
