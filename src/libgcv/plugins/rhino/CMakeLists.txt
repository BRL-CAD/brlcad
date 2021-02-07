set(RHINO_SRCS
  rhino_read.cpp
  )

gcv_plugin_library(gcv-rhino SHARED ${RHINO_SRCS})
target_link_libraries(gcv-rhino librt libwdb libbg ${OPENNURBS_LIBRARIES})
set_property(TARGET gcv-rhino APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(gcv-rhino "${RHINO_SRCS}")
PLUGIN_SETUP(gcv-rhino gcv)

CMAKEFILES(
  ${RHINO_SRCS}
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

