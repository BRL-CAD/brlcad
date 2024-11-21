include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(
  PSCALE_SRCS
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

ged_plugin_library(ged-pscale ${PSCALE_SRCS})
target_link_libraries(ged-pscale libged libbu)

cmakefiles(
  CMakeLists.txt
  ${PSCALE_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
