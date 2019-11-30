set(analyze_headers
  defines.h
  debug.h
  density.h
  nirt.h
  )
BRLCAD_MANAGE_FILES(analyze_headers ${INCLUDE_DIR}/brlcad/analyze)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
