add_subdirectory(view)

set(ged_headers
  analyze.h
  commands.h
  database.h
  debug.h
  defines.h
  framebuffer.h
  objects.h
  rt.h
  view.h
  )
BRLCAD_MANAGE_FILES(ged_headers ${INCLUDE_DIR}/brlcad/ged)

BRLCAD_CHECK_HDRS(ged_headers)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
