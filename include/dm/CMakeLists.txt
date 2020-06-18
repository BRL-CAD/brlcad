set(dm_headers
  bview.h
  defines.h
  dm_xvars.h
  )
BRLCAD_MANAGE_FILES(dm_headers ${INCLUDE_DIR}/brlcad/dm)

CMAKEFILES(dm-design.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
