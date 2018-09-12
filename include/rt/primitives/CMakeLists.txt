set(primitives_headers
  arb8.h
  annot.h
  bot.h
  brep.h
  cline.h
  dsp.h
  ell.h
  epa.h
  hf.h
  metaball.h
  pg.h
  pipe.h
  rhc.h
  rpc.h
  script.h
  sketch.h
  tgc.h
  tor.h
  )
BRLCAD_MANAGE_FILES(primitives_headers ${INCLUDE_DIR}/brlcad/rt/primitives)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
