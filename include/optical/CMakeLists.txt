set(optical_headers
  defines.h
  debug.h
  light.h
  plastic.h
  shadework.h
  shadefuncs.h
  )
BRLCAD_MANAGE_FILES(optical_headers ${INCLUDE_DIR}/brlcad/optical)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
