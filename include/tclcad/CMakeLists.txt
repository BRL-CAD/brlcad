set(tclcad_headers
  defines.h
  misc.h
  )
BRLCAD_MANAGE_FILES(tclcad_headers ${INCLUDE_DIR}/brlcad/tclcad)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
