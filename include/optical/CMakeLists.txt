set(
  optical_headers
  defines.h
  debug.h
  light.h
  plastic.h
  shadework.h
  shadefuncs.h
)
brlcad_manage_files(optical_headers ${INCLUDE_DIR}/brlcad/optical REQUIRED liboptical)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
