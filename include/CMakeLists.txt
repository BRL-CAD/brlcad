add_subdirectory(bu)
add_subdirectory(dm)
add_subdirectory(rt)

set(public_hdrs
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
CMAKEFILES(${public_hdrs})
install(FILES ${public_hdrs} DESTINATION ${INCLUDE_DIR}/brlcad)

# Configuration headers
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/brlcad_config.h DESTINATION ${INCLUDE_DIR}/brlcad)
if(WIN32)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config_win.h DESTINATION ${INCLUDE_DIR}/brlcad)
endif(WIN32)

# headers used by multiple packages but still considered private.
# public headers should NOT include these headers.
set(noinst_HEADERS
  bin.h
  bio.h
  brlcad_ident.h
  brlcad_version.h
  bselect.h
  conf.h
  fbmsg.h
  sysv.h
  )
CMAKEFILES(${noinst_HEADERS})

# Enforce the above list - check the public headers to make sure
# they do not include any private headers.
#set(inc_error 0)
#set(msgs "")
#foreach(header_file ${public_hdrs})
#  file(READ "${header_file}" contents)
#  foreach(noinst_file ${noinst_HEADERS})
#    if("${contents}" MATCHES "include [\"<]${noinst_file}")
#      set(inc_error 1)
#      set(msgs "${msgs}\nPublic header file ${header_file} is including private header ${noinst_file}")
#    endif("${contents}" MATCHES "include [\"<]${noinst_file}")
#  endforeach(noinst_file ${noinst_HEADERS})
#endforeach(header_file ${public_hdrs})
#if(inc_error)
#  message(FATAL_ERROR "${msgs}\nFATAL ERROR: Public headers are including private headers per messages above.")
#endif(inc_error)

set(include_misc
  conf
  config_win.h.in
  )
CMAKEFILES(${include_misc})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
