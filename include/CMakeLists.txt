add_subdirectory(bu)
add_subdirectory(dm)
add_subdirectory(rt)

set(include_hdrs
  ${CMAKE_CURRENT_BINARY_DIR}/brlcad_config.h
  RtServerImpl.h
  bn.h
  bu.h
  common.h
  cursor.h
  ged.h
  pstdint.h
  raytrace.h
  rtfunc.h
  rtgeom.h
  rtserver.h
  tclcad.h
  analyze.h
  anim.h
  bot.h
  brep.h
  brlcad.h
  brlcad.i
  db.h
  db5.h
  dg.h
  dm.h
  dvec.h
  fb.h
  fbio.h
  fbserv_obj.h
  fft.h
  gcv.h
  icv.h
  libtermio.h
  light.h
  mater.h
  nmg.h
  nurb.h
  obj.h
  optical.h
  orle.h
  photonmap.h
  pc.h
  pkg.h
  plastic.h
  plot3.h
  redblack.h
  shadefuncs.h
  shadework.h
  solid.h
  spectrum.h
  spm.h
  tie.h
  tol.h
  vectfont.h
  vector_fpu.h
  vector_x86.h
  vmath.h
  wdb.h
  )
if(WIN32)
  set(include_hdrs ${include_hdrs} ${CMAKE_CURRENT_BINARY_DIR}/config_win.h)
endif(WIN32)
install(FILES ${include_hdrs} DESTINATION ${INCLUDE_DIR}/brlcad)

# headers used by multiple packages but still considered private.
# public headers should NOT include these headers.
set(noinst_HEADERS
  bin.h
  bio.h
  brlcad_version.h
  bselect.h
  conf.h
  db_diff.h
  fbmsg.h
  sysv.h
  )

set(include_misc
  conf
  config_win.h.in
  )

CMAKEFILES(${include_hdrs})
CMAKEFILES(${noinst_HEADERS})
CMAKEFILES(${include_misc})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
