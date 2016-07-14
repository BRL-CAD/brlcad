set(dm_headers
  bview.h
  defines.h
  dm_xvars.h
  )

install(FILES ${dm_headers} DESTINATION ${INCLUDE_DIR}/brlcad/dm)

CMAKEFILES(${dm_headers} dm-design.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
