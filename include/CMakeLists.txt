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
install(FILES ${public_hdrs} DESTINATION ${INCLUDE_DIR}/brlcad)

# Configuration headers
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/brlcad_config.h" DESTINATION ${INCLUDE_DIR}/brlcad)
if(WIN32)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/config_win.h" DESTINATION ${INCLUDE_DIR}/brlcad)
endif(WIN32)

# Used by others but still considered private
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/brlcad_version.h" DESTINATION ${INCLUDE_DIR})

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
  brlcad_ident.h.in
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
