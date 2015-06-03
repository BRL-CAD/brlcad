set(optical_headers
  defines.h
  debug.h
  light.h
  plastic.h
  shadework.h
  shadefuncs.h
  )

install(FILES ${optical_headers} DESTINATION ${INCLUDE_DIR}/brlcad/optical)

CMAKEFILES(${optical_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
