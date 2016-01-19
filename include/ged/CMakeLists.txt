add_subdirectory(view)

set(ged_headers
  analyze.h
  database.h
  debug.h
  defines.h
  framebuffer.h
  objects.h
  rt.h
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
