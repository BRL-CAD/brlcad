find_package_zlib(REQUIRED)

set(BLEND_SRCS
  blend_read.cpp
  )

gcv_plugin_library(gcv-blend SHARED ${BLEND_SRCS})
target_link_libraries(gcv-blend libgcv libwdb librt libbg libbu ZLIB::ZLIB)
set_property(TARGET gcv-blend APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-blend "${BLEND_SRCS}")
plugin_setup(gcv-blend gcv)

cmakefiles(
  ${BLEND_SRCS}
  CMakeLists.txt
  README
  VALIDATION.md
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
