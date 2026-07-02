set(SCAD_SRCS
  scad_parse.c
  scad_eval.c
  scad_geom.c
  scad_read.c
)

gcv_plugin_library(gcv-scad SHARED ${SCAD_SRCS})
target_link_libraries(gcv-scad libgcv libwdb librt libbg libbn libbu)
set_property(TARGET gcv-scad APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-scad "${SCAD_SRCS}")
plugin_setup(gcv-scad gcv)

cmakefiles(
  ${SCAD_SRCS}
  scad.h
  CMakeLists.txt
  TODO.scad
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
