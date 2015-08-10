set(gcv_headers
  defines.h
  api.h
  util.h
  )

install(FILES ${gcv_headers} DESTINATION ${INCLUDE_DIR}/brlcad/gcv)

CMAKEFILES(${gcv_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
