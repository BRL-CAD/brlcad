set(dm_headers
  defines.h
  fbserv.h
  util.h
  view.h
  )
BRLCAD_MANAGE_FILES(dm_headers ${INCLUDE_DIR}/brlcad/dm)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
