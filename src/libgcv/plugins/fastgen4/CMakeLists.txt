set(FASTGEN4_SRCS fastgen4_read.c fastgen4_write.cpp)

gcv_plugin_library(gcv-fastgen4 SHARED ${FASTGEN4_SRCS})
target_link_libraries(gcv-fastgen4 libgcv libwdb librt libbu)
set_property(TARGET gcv-fastgen4 APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-fastgen4 "${FASTGEN4_SRCS}")
plugin_setup(gcv-fastgen4 gcv)

cmakefiles(
  ${FASTGEN4_SRCS}
  CMakeLists.txt
  NOTES
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
