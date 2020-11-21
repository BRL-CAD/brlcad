set(DECIMATE_SRCS
  decimate.c
  )

gcv_plugin_library(gcv-decimate SHARED ${DECIMATE_SRCS})
target_link_libraries(gcv-decimate librt)
set_property(TARGET gcv-decimate APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(gcv-decimate "${DECIMATE_SRCS}")
PLUGIN_SETUP(gcv-decimate gcv)

CMAKEFILES(
  ${DECIMATE_SRCS}
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

