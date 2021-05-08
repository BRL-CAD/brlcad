set(VOL_SRCS
  png_read.c
  )

gcv_plugin_library(gcv-vol SHARED ${VOL_SRCS})
target_link_libraries(gcv-vol libwdb librt libbu libicv)
set_property(TARGET gcv-vol APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(gcv-vol "${VOL_SRCS}")
PLUGIN_SETUP(gcv-vol gcv)

CMAKEFILES(
  ${VOL_SRCS}
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

