include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(edit_srcs
  edit.c
  edit2.cpp
  protate.c
  pscale.c
  ptranslate.c
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

ged_plugin_library(ged-edit ${edit_srcs})
target_link_libraries(ged-edit libged libbu)

cmakefiles(
  CMakeLists.txt
  ${edit_srcs}
  pscale.h
  ged_edit.h
  ged_edit2.h
  uri.hh
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
