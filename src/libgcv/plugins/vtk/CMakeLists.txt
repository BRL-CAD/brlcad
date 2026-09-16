set(VTK_SRCS vtk_read.c)

gcv_plugin_library(gcv-vtk SHARED ${VTK_SRCS})
target_link_libraries(gcv-vtk libgcv libwdb librt libbu libbg)
set_property(TARGET gcv-vtk APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-vtk "${VTK_SRCS}")
plugin_setup(gcv-vtk gcv)

cmakefiles(
  ${VTK_SRCS}
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
