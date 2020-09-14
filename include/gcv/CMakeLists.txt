set(gcv_headers
  defines.h
  api.h
  util.h
  )
BRLCAD_MANAGE_FILES(gcv_headers ${INCLUDE_DIR}/brlcad/gcv)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
