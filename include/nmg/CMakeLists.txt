set(nmg_headers
  debug.h
  )
BRLCAD_MANAGE_FILES(nmg_headers ${INCLUDE_DIR}/brlcad/nmg)

BRLCAD_CHECK_HDRS(nmg_headers)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
