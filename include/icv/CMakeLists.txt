set(icv_headers
  defines.h
  colorspace.h
  crop.h
  filters.h
  io.h
  ops.h
  stat.h
  )
BRLCAD_MANAGE_FILES(icv_headers ${INCLUDE_DIR}/brlcad/icv)

BRLCAD_CHECK_HDRS(icv_headers)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
