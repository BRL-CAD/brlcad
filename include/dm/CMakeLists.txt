set(dm_headers
  defines.h
  dm_xvars.h
  )

install(FILES ${dm_headers} DESTINATION ${INCLUDE_DIR}/brlcad/dm)

CMAKEFILES(${dm_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
