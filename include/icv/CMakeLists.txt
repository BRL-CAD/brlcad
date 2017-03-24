set(icv_headers
  defines.h
  colorspace.h
  crop.h
  filters.h
  io.h
  ops.h
  stat.h
  )

install(FILES ${icv_headers} DESTINATION ${INCLUDE_DIR}/brlcad/icv)

CMAKEFILES(${icv_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
