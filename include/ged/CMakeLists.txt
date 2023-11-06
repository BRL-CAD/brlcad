add_subdirectory(view)

set(ged_headers
  analyze.h
  commands.h
  database.h
  dbi.h
  debug.h
  defines.h
  framebuffer.h
  objects.h
  rt.h
  view.h
  )
BRLCAD_MANAGE_FILES(ged_headers ${INCLUDE_DIR}/brlcad/ged)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
