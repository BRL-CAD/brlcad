set(ged_headers
  defines.h
  database.h
  objects.h
  framebuffer.h
  view.h
  )

install(FILES ${ged_headers} DESTINATION ${INCLUDE_DIR}/brlcad/ged)

CMAKEFILES(${ged_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
