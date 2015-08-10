set(primitives_headers
  arb8.h
  ars.h
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
  sketch.h
  tgc.h
  tor.h)

install(FILES ${primitives_headers} DESTINATION ${INCLUDE_DIR}/brlcad/rt/primitives)

CMAKEFILES(${primitives_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
