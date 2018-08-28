add_subdirectory(bu)
add_subdirectory(bn)
add_subdirectory(bg)
add_subdirectory(brep)
add_subdirectory(dm)
add_subdirectory(gcv)
add_subdirectory(ged)
add_subdirectory(icv)
add_subdirectory(optical)
add_subdirectory(rt)
add_subdirectory(fb)

set(public_hdrs
  RtServerImpl.h
  analyze.h
  bg.h
  bio.h
  bn.h
  bnetwork.h
  brep.h
  bresource.h
  brlcad.h
  brlcad.i
  bsocket.h
  bu.h
  common.h
  cursor.h
  dm.h
  fb.h
  fft.h
  gcv.h
  ged.h
  icv.h
  libtermio.h
  nmg.h
  optical.h
  pc.h
  photonmap.h
  pinttypes.h
  pkg.h
  pstdint.h
  raytrace.h
  rtserver.h
  tclcad.h
  vmath.h
  wdb.h
  )
BRLCAD_MANAGE_FILES("${public_hdrs}" ${INCLUDE_DIR}/brlcad)

BUILD_CFG_HDR(brlcad_ident.h.in ${INCLUDE_DIR}/brlcad)
BUILD_CFG_HDR(brlcad_version.h.in ${INCLUDE_DIR}/brlcad)

# FIXME: shouldn't exist, needs to be absorbed into brlcad_config.h
if(MSVC)
  BUILD_CFG_HDR(config_win.h.in ${INCLUDE_DIR}/brlcad)
endif(MSVC)

set(include_misc
  brlcad_ident.h.in
  brlcad_version.h.in
  conf/BrlcadConfig.tmpl
  conf/MAJOR
  conf/MINOR
  conf/PATCH
  conf/make.vbs
  config_win.h.in
  )
CMAKEFILES(${include_misc})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
