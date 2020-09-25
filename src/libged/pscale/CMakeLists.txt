include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(PSCALE_SRCS
  pscale.c
  scale_ehy.c
  scale_ell.c
  scale_epa.c
  scale_eto.c
  scale_extrude.c
  scale_hyp.c
  scale_part.c
  scale_rhc.c
  scale_rpc.c
  scale_superell.c
  scale_tgc.c
  scale_tor.c
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-pscale SHARED ${PSCALE_SRCS})
target_link_libraries(ged-pscale libged libbu)
set_property(TARGET ged-pscale APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-pscale ${PSCALE_SRCS})
PLUGIN_SETUP(ged-pscale ged)

CMAKEFILES(
  CMakeLists.txt
  ${PSCALE_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

