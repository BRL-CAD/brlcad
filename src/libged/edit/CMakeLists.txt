set(PLUGIN_LIBS libged libbu)

set(edit_srcs
  edit.c
  edit2.cpp
  protate.c
  pscale.c
  ptranslate.c
  pscale.c
  rotate_eto.c
  rotate_extrude.c
  rotate_hyp.c
  rotate_tgc.c
  scale_ehy.c
  scale_ell.c
  scale_epa.c
  scale_eto.c
  scale_extrude.c
  scale_hyp.c
  scale_metaball.c
  scale_part.c
  scale_pipe.c
  scale_rhc.c
  scale_rpc.c
  scale_superell.c
  scale_tgc.c
  scale_tor.c
  translate_extrude.c
  translate_tgc.c
  edarb.c
  facedef.c
)

ged_plugin_library(ged-edit
  ${edit_srcs} 
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ${edit_srcs}
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
